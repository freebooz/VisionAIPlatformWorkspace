package httptransport

import (
	"encoding/json"
	"errors"
	"io"
	"net/http"

	application "visionai/controlplane/internal/application/controlplane"
	"visionai/controlplane/internal/domain/edge"
	"visionai/controlplane/internal/domain/event"
)

const maxRequestBodyBytes int64 = 1 << 20

// Server（HTTP 传输层）只负责协议转换，不直接访问数据库/内存仓储实现。
type Server struct {
	app *application.Service
}

// NewServer（创建 HTTP 服务器）在启动阶段校验应用服务依赖。
func NewServer(app *application.Service) *Server {
	if app == nil {
		panic("controlplane application service must not be nil")
	}
	return &Server{app: app}
}

// edgeNodeDTO（边缘节点传输对象）隔离 HTTP JSON 结构与领域实体。
type edgeNodeDTO struct {
	ID     string `json:"id"`
	Name   string `json:"name"`
	Status string `json:"status"`
}

// eventDTO（事件传输对象）隔离外部 JSON 与 domain/event（领域事件）。
type eventDTO struct {
	ID        string `json:"id"`
	Type      string `json:"type"`
	StreamID  uint64 `json:"streamId"`
	Timestamp int64  `json:"timestamp"`
}

// Handler（HTTP 路由）创建无全局可变状态的 ServeMux（路由器）。
func (s *Server) Handler() http.Handler {
	mux := http.NewServeMux()

	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, _ *http.Request) {
		writeJSON(w, http.StatusOK, map[string]any{
			"service": "VisionAIControlPlane",
			"status":  "ok",
		})
	})

	mux.HandleFunc("GET /api/v1/edge-nodes", func(w http.ResponseWriter, r *http.Request) {
		nodes, err := s.app.ListEdgeNodes(r.Context())
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "list edge nodes failed"})
			return
		}

		result := make([]edgeNodeDTO, 0, len(nodes))
		for _, node := range nodes {
			result = append(result, edgeNodeDTO{
				ID: node.ID, Name: node.Name, Status: node.Status,
			})
		}
		writeJSON(w, http.StatusOK, result)
	})

	mux.HandleFunc("POST /api/v1/edge-nodes", func(w http.ResponseWriter, r *http.Request) {
		var request edgeNodeDTO
		if err := decodeJSON(w, r, &request); err != nil {
			writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid edge node"})
			return
		}

		node := edge.Node{
			ID: request.ID, Name: request.Name, Status: request.Status,
		}
		if err := s.app.RegisterEdgeNode(r.Context(), node); err != nil {
			writeJSON(w, http.StatusBadRequest, map[string]string{"error": err.Error()})
			return
		}

		if node.Status == "" {
			node.Status = "online"
		}
		writeJSON(w, http.StatusCreated, edgeNodeDTO{
			ID: node.ID, Name: node.Name, Status: node.Status,
		})
	})

	mux.HandleFunc("GET /api/v1/events", func(w http.ResponseWriter, r *http.Request) {
		events, err := s.app.ListEvents(r.Context())
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "list events failed"})
			return
		}

		result := make([]eventDTO, 0, len(events))
		for _, item := range events {
			result = append(result, eventDTO{
				ID: item.ID, Type: item.Type, StreamID: item.StreamID, Timestamp: item.Timestamp,
			})
		}
		writeJSON(w, http.StatusOK, result)
	})

	mux.HandleFunc("POST /api/v1/events", func(w http.ResponseWriter, r *http.Request) {
		var request eventDTO
		if err := decodeJSON(w, r, &request); err != nil {
			writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid event"})
			return
		}

		item := event.Event{
			ID: request.ID, Type: request.Type, StreamID: request.StreamID, Timestamp: request.Timestamp,
		}
		if err := s.app.RecordEvent(r.Context(), item); err != nil {
			writeJSON(w, http.StatusBadRequest, map[string]string{"error": err.Error()})
			return
		}
		writeJSON(w, http.StatusCreated, request)
	})

	return mux
}

// decodeJSON（安全 JSON 解码）限制请求体大小、拒绝未知字段和尾随第二个 JSON 对象。
// 这样可降低误配置、协议漂移和超大请求体造成的内存压力。
func decodeJSON(w http.ResponseWriter, r *http.Request, target any) error {
	r.Body = http.MaxBytesReader(w, r.Body, maxRequestBodyBytes)

	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()

	if err := decoder.Decode(target); err != nil {
		return err
	}

	var extra any
	if err := decoder.Decode(&extra); !errors.Is(err, io.EOF) {
		if err == nil {
			return errors.New("request body must contain a single JSON value")
		}
		return err
	}

	return nil
}

// writeJSON（统一 JSON 响应）集中设置安全响应头和 Content-Type（内容类型）。
func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

