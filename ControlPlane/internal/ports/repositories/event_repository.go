package repositories

import (
	"context"

	"visionai/controlplane/internal/domain/event"
)

// EventRepository（事件仓储端口）定义事件存储抽象。
type EventRepository interface {
	Append(ctx context.Context, item event.Event) error
	List(ctx context.Context) ([]event.Event, error)
}

