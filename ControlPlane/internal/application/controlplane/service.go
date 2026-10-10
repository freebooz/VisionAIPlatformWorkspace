package controlplane

import (
	"context"
	"errors"
	"strings"

	"visionai/controlplane/internal/domain/edge"
	"visionai/controlplane/internal/domain/event"
	"visionai/controlplane/internal/ports/repositories"
)

const (
	maxNodeIDLength    = 128
	maxNodeNameLength  = 256
	maxNodeStatusLength = 64
	maxEventIDLength   = 128
	maxEventTypeLength = 128
)

// Service（中心控制应用服务）编排用例，不依赖 HTTP 或具体数据库。
type Service struct {
	edgeNodes repositories.EdgeNodeRepository
	events    repositories.EventRepository
}

// NewService（创建应用服务）在启动阶段校验依赖，避免运行中出现 nil 接口导致 Panic（崩溃）。
func NewService(
	edgeNodes repositories.EdgeNodeRepository,
	events repositories.EventRepository,
) *Service {
	if edgeNodes == nil || events == nil {
		panic("controlplane repositories must not be nil")
	}

	return &Service{
		edgeNodes: edgeNodes,
		events:    events,
	}
}

// RegisterEdgeNode（注册/更新边缘节点）执行轻量领域输入校验，再调用仓储 Port（端口）。
func (s *Service) RegisterEdgeNode(ctx context.Context, node edge.Node) error {
	node.ID = strings.TrimSpace(node.ID)
	node.Name = strings.TrimSpace(node.Name)
	node.Status = strings.TrimSpace(node.Status)

	if node.ID == "" || len(node.ID) > maxNodeIDLength {
		return errors.New("edge node id is required")
	}
	if len(node.Name) > maxNodeNameLength || len(node.Status) > maxNodeStatusLength {
		return errors.New("edge node fields exceed length limit")
	}
	if node.Status == "" {
		node.Status = "online"
	}
	return s.edgeNodes.Upsert(ctx, node)
}

// ListEdgeNodes（查询边缘节点）只依赖仓储接口，不感知具体数据库实现。
func (s *Service) ListEdgeNodes(ctx context.Context) ([]edge.Node, error) {
	return s.edgeNodes.List(ctx)
}

// RecordEvent（记录事件）限制关键字符串长度，防止异常客户端向内存/数据库写入超大字段。
func (s *Service) RecordEvent(ctx context.Context, item event.Event) error {
	item.ID = strings.TrimSpace(item.ID)
	item.Type = strings.TrimSpace(item.Type)

	if item.ID == "" || len(item.ID) > maxEventIDLength {
		return errors.New("event id is required")
	}
	if item.Type == "" || len(item.Type) > maxEventTypeLength {
		return errors.New("event type is required")
	}
	return s.events.Append(ctx, item)
}

// ListEvents（查询事件）返回仓储层提供的稳定快照。
func (s *Service) ListEvents(ctx context.Context) ([]event.Event, error) {
	return s.events.List(ctx)
}

