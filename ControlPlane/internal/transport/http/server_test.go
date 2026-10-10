package httptransport

import (
	"net/http"
	"net/http/httptest"
	"testing"

	application "visionai/controlplane/internal/application/controlplane"
	"visionai/controlplane/internal/adapters/memory"
)

// TestHealth（健康检查测试）验证传输层可以独立组装应用服务并返回 HTTP 200。
func TestHealth(t *testing.T) {
	repositories := memory.NewRepositories()
	app := application.NewService(repositories, memory.NewEventRepository(repositories))
	server := NewServer(app)

	req := httptest.NewRequest(http.MethodGet, "/healthz", nil)
	rec := httptest.NewRecorder()

	server.Handler().ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rec.Code)
	}
}

