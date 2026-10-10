package event

// Event（事件）是中心控制平面的领域实体。
type Event struct {
	ID        string
	Type      string
	StreamID  uint64
	Timestamp int64
}

