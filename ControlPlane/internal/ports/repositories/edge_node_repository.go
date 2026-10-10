package repositories

import (
	"context"

	"visionai/controlplane/internal/domain/edge"
)

// EdgeNodeRepository（边缘节点仓储端口）定义应用层需要的持久化能力。
type EdgeNodeRepository interface {
	Upsert(ctx context.Context, node edge.Node) error
	List(ctx context.Context) ([]edge.Node, error)
}

