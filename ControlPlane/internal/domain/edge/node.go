package edge

// Node（边缘节点）是中心控制平面的领域实体。
// 领域层只表达业务概念，不依赖 HTTP、数据库或消息中间件。
type Node struct {
	ID     string
	Name   string
	Status string
}

