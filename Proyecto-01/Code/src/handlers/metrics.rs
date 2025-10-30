use crate::server::requests::HttpRequest;
use crate::server::response::{HttpResponse, JsonResponseBuilder};
use crate::error::ServerResult;
use crate::utils::metrics::metrics;
use crate::workers::worker_manager::worker_manager;

pub fn handle_metrics(_req: &HttpRequest) -> ServerResult<HttpResponse> {
    let snap = metrics().lock().unwrap().snapshot();
    let pool_stats = worker_manager().pool_stats();
    let mut items: Vec<String> = Vec::new();
    for (route, s) in snap.iter() {
        // find pool info if exists
        let mut qlen = 0usize; let mut qcap = 0usize; let mut workers = 0usize;
        if let Some(ps) = pool_stats.iter().find(|p| p.route == *route) {
            qlen = ps.queue_len; qcap = ps.queue_capacity; workers = ps.workers;
        }
        items.push(format!(
            r#"{{"route":"{}","requests":{},"successes":{},"failures":{},"s2xx":{},"s4xx":{},"s5xx":{},"avg_us":{},"min_us":{},"max_us":{},"queue_len":{},"queue_cap":{},"workers":{}}}"#,
            route,
            s.requests,
            s.successes,
            s.failures,
            s.s2xx,
            s.s4xx,
            s.s5xx,
            if s.requests>0 { s.total_us / s.requests as u128 } else { 0 },
            s.min_us,
            s.max_us,
            qlen,
            qcap,
            workers
        ));
    }
    let json = format!("[{}]", items.join(","));
    Ok(JsonResponseBuilder::new(200).field_raw("routes", json).build())
}
