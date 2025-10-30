//! HTTP/1.0 Server - Main Entry Point

use http_server::{ConfigBuilder, HttpServer, Router, ServerResult};
use http_server::handlers::basics;
use http_server::handlers::{set_available_routes, set_data_dir};
use http_server::utils::logging;
use http_server::workers::init_global_worker_manager;
use std::env;

fn main() -> ServerResult<()> {
    // Parse command line arguments
    let args: Vec<String> = env::args().collect();
    let mut config_builder = ConfigBuilder::new();

    // Simple argument parsing
    let mut i = 1;
    while i < args.len() {
        if args[i].starts_with("--port=") {
            if let Some(port_str) = args[i].strip_prefix("--port=") {
                if let Ok(port) = port_str.parse::<u16>() {
                    config_builder = config_builder.port(port);
                }
            }
        }
        i += 1;
    }

    let config = config_builder.build()?;

    // Initialize logging
    logging::init_logger(config.enable_logging);

    // Initialize worker system (CPU pool, etc.)
    init_global_worker_manager(&config);

    // Build router with all handlers
    let router = Router::new()
        .route("/timestamp", basics::handle_timestamp)
        .route("/reverse", basics::handle_reverse)
        .route("/toupper", basics::handle_toupper)
        .route("/hash", basics::handle_hash)
        .route("/random", basics::handle_random)
        .route("/fibonacci", basics::handle_fibonacci)
        .route("/createfile", basics::handle_createfile)
        .route("/deletefile", basics::handle_deletefile)
        .route("/status", basics::handle_status)
        .route("/help", basics::handle_help)
        // CPU-bound endpoints
        .route("/isprime", http_server::handlers::handle_isprime)
        .route("/factor", http_server::handlers::handle_factor)
        .route("/pi", http_server::handlers::handle_pi)
        .route("/mandelbrot", http_server::handlers::handle_mandelbrot)
        .route("/matrixmul", http_server::handlers::handle_matrixmul)
        // IO-bound endpoints
        .route("/sortfile", http_server::handlers::handle_sortfile)
        .route("/wordcount", http_server::handlers::handle_wordcount)
        .route("/grep", http_server::handlers::handle_grep)
        .route("/compress", http_server::handlers::handle_compress)
        .route("/hashfile", http_server::handlers::handle_hashfile);

    // Snapshot available routes for dynamic /help
    let routes_list = router
        .routes()
        .into_iter()
        .map(|s| s.to_string())
        .collect();
    set_available_routes(routes_list);

    // Publish data_dir for IO handlers
    set_data_dir(config.data_dir.to_string_lossy().to_string());

    // Create and start server
    let mut server = HttpServer::new(config, router);

    println!("Starting HTTP/1.0 Server...");
    println!("Listening on http://{}", server.stats().bind_address);
    println!("Press Ctrl+C to stop");

    server.start()?;

    Ok(())
}
