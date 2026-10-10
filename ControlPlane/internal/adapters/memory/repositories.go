package memory

import (
	"context"
	"errors"
	"sort"
	"sync"

	"visionai/controlplane/internal/domain/edge"
	"visionai/controlplane/internal/domain/event"
)

var (
	// ErrNodeCapacity（节点容量已满）用于防止开发态内存仓储被无限动态 ID 撑大。
	ErrNodeCapacity = errors.New("edge node repository capacity exceeded")
)

const (
	defaultMaxNodes  = 10_000
	defaultMaxEvents = 100_000
)

// Repositories（内存仓储适配器）仅用于开发、自测和首期工程基线。
// 生产环境可替换为 PostgreSQL / Redis 等实现，而不修改 application（应用层）。
type Repositories struct {
	mu sync.RWMutex

	nodes    map[string]edge.Node
	maxNodes int

	// events 使用固定容量环形缓冲，避免长期运行后事件 slice 无限增长。
	events     []event.Event
	eventHead  int
	eventCount int
	maxEvents  int
}

// NewRepositories（创建内存仓储）使用安全默认容量，适合开发和自动测试。
func NewRepositories() *Repositories {
	return NewRepositoriesWithLimits(defaultMaxNodes, defaultMaxEvents)
}

// NewRepositoriesWithLimits（按容量创建内存仓储）允许测试环境使用更小边界验证淘汰逻辑。
func NewRepositoriesWithLimits(maxNodes, maxEvents int) *Repositories {
	if maxNodes <= 0 || maxEvents <= 0 {
		panic("memory repository limits must be greater than zero")
	}

	return &Repositories{
		nodes:     map[string]edge.Node{},
		maxNodes:  maxNodes,
		events:    make([]event.Event, maxEvents),
		maxEvents: maxEvents,
	}
}

// Upsert（新增或更新边缘节点）尊重 Context（上下文）取消，并限制唯一节点总数。
func (r *Repositories) Upsert(ctx context.Context, node edge.Node) error {
	if err := ctx.Err(); err != nil {
		return err
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	if _, exists := r.nodes[node.ID]; !exists && len(r.nodes) >= r.maxNodes {
		return ErrNodeCapacity
	}

	r.nodes[node.ID] = node
	return nil
}

// List（列出边缘节点）返回独立快照，并按 ID 排序保证 API 和测试结果稳定。
func (r *Repositories) List(ctx context.Context) ([]edge.Node, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	r.mu.RLock()
	defer r.mu.RUnlock()

	result := make([]edge.Node, 0, len(r.nodes))
	for _, node := range r.nodes {
		result = append(result, node)
	}
	sort.Slice(result, func(i, j int) bool {
		return result[i].ID < result[j].ID
	})
	return result, nil
}

// Append（追加事件）实现 EventRepository（事件仓储端口）的底层环形缓存写入。
// 达到容量后覆盖最旧事件，从而保证开发态服务长期运行时内存有明确上界。
func (r *Repositories) Append(ctx context.Context, item event.Event) error {
	if err := ctx.Err(); err != nil {
		return err
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	if r.eventCount < r.maxEvents {
		index := (r.eventHead + r.eventCount) % r.maxEvents
		r.events[index] = item
		r.eventCount++
		return nil
	}

	r.events[r.eventHead] = item
	r.eventHead = (r.eventHead + 1) % r.maxEvents
	return nil
}

// ListEvents 与边缘节点 List 同名会产生 Go 方法冲突，因此事件仓储由专用适配器包装。
func (r *Repositories) ListEvents(ctx context.Context) ([]event.Event, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	r.mu.RLock()
	defer r.mu.RUnlock()

	result := make([]event.Event, 0, r.eventCount)
	for i := 0; i < r.eventCount; i++ {
		index := (r.eventHead + i) % r.maxEvents
		result = append(result, r.events[index])
	}
	return result, nil
}

// EventRepositoryAdapter（事件仓储适配器）复用同一内存状态，同时满足独立端口。
type EventRepositoryAdapter struct {
	root *Repositories
}

// NewEventRepository（创建事件仓储适配器）复用 Repositories 的同步和环形缓存状态。
func NewEventRepository(root *Repositories) *EventRepositoryAdapter {
	if root == nil {
		panic("memory repositories must not be nil")
	}
	return &EventRepositoryAdapter{root: root}
}

// Append（追加事件）转发到共享内存仓储。
func (a *EventRepositoryAdapter) Append(ctx context.Context, item event.Event) error {
	return a.root.Append(ctx, item)
}

// List（列出事件）返回按写入时间排序的事件快照。
func (a *EventRepositoryAdapter) List(ctx context.Context) ([]event.Event, error) {
	return a.root.ListEvents(ctx)
}

