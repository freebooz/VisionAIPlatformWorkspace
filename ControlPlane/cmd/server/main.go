package main

import (
	"context"
	"errors"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	application "visionai/controlplane/internal/application/controlplane"
	"visionai/controlplane/internal/adapters/memory"
	httptransport "visionai/controlplane/internal/transport/http"
)

// main（中心控制平面进程入口）负责依赖装配、HTTP Server（服务器）安全参数和优雅停机。
func main() {
	repositories := memory.NewRepositories()
	app := application.NewService(
		repositories,
		memory.NewEventRepository(repositories),
	)
	transport := httptransport.NewServer(app)

	const addr = ":8080"
	log.Printf("VisionAIControlPlane（机器视觉 AI 中心控制平面）启动: http://127.0.0.1%s", addr)

	// 不直接使用 http.ListenAndServe 默认值，因为默认 Server 没有任何读写超时，
	// 在公网或复杂专网环境中容易被慢连接长期占用文件描述符和 Goroutine（协程）。
	server := &http.Server{
		Addr:              addr,
		Handler:           transport.Handler(),
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      30 * time.Second,
		IdleTimeout:       60 * time.Second,
		MaxHeaderBytes:    1 << 20,
	}

	errCh := make(chan error, 1)
	go func() {
		errCh <- server.ListenAndServe()
	}()

	// 监听 Ctrl+C / SIGTERM（终止信号），收到后先停止接收新请求，再等待在途请求结束。
	signalContext, stop := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer stop()

	select {
	case <-signalContext.Done():
		log.Printf("收到停止信号，开始优雅关闭 VisionAIControlPlane")
	case err := <-errCh:
		if err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Fatalf("VisionAIControlPlane 启动/运行失败: %v", err)
		}
		return
	}

	shutdownContext, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := server.Shutdown(shutdownContext); err != nil {
		log.Printf("VisionAIControlPlane 优雅关闭超时或失败: %v", err)
		_ = server.Close()
	}
}
