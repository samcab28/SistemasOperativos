import http from 'k6/http';
import { check, sleep } from 'k6';
import { Counter, Trend } from 'k6/metrics';

const BASE_URL = __ENV.MASTER_URL || 'http://127.0.0.1:8080';

// Custom metrics
const eventsProcessed = new Counter('events_processed');
const batchLatency = new Trend('batch_latency_ms');

export const options = {
  scenarios: {
    high_throughput: {
      executor: 'constant-vus',
      vus: 5,
      duration: '60s',
    },
  },
  thresholds: {
    events_processed: ['count>5000'], // Process at least 5k events/s
    batch_latency_ms: ['p(95)<1000'], // 95% under 1s
  },
};

let topologyId;

export function setup() {
  // Create a persistent topology
  const topology = {
    name: 'throughput-test',
    operators: [
      {
        id: 'keyby',
        kind: {
          type: 'key_by',
          field: 'service'
        },
        config: {}
      },
      {
        id: 'window',
        kind: {
          type: 'window_aggregate',
          window: {
            length_ms: 60000,
            checkpoint_interval_ms: 30000
          },
          aggregator: { type: 'count' },
          key_field: 'service'
        },
        config: {}
      },
      {
        id: 'sink',
        kind: { type: 'sink_log' },
        config: {}
      }
    ],
    edges: [],
    parallelism: 1
  };

  const res = http.post(
    `${BASE_URL}/api/v1/topologies`,
    JSON.stringify(topology),
    { headers: { 'Content-Type': 'application/json' } }
  );

  if (res.status !== 200) {
    throw new Error(`Failed to create topology: ${res.status}`);
  }

  const id = JSON.parse(res.body).topology_id;
  console.log(`Topology created: ${id}`);
  
  sleep(3); // Wait for deployment

  return { topologyId: id };
}

export default function(data) {
  // Generate large batch of events
  const batchSize = 100;
  const events = [];
  
  for (let i = 0; i < batchSize; i++) {
    events.push({
      timestamp: new Date().toISOString(),
      data: {
        service: `service-${i % 10}`,
        request_id: `req-${Date.now()}-${i}`,
        latency_ms: Math.random() * 100,
        status: i % 5 === 0 ? 'error' : 'success'
      }
    });
  }

  const payload = {
    topology_id: data.topologyId,
    events: events
  };

  const start = Date.now();
  const res = http.post(
    `${BASE_URL}/api/v1/ingest`,
    JSON.stringify(payload),
    { 
      headers: { 'Content-Type': 'application/json' },
      timeout: '5s'
    }
  );
  const duration = Date.now() - start;

  const success = check(res, {
    'ingest successful': (r) => r.status === 202,
  });

  if (success) {
    eventsProcessed.add(batchSize);
    batchLatency.add(duration);
  } else {
    console.error(`Ingest failed: ${res.status} ${res.body}`);
  }

  sleep(0.1); // Small delay between batches
}

export function teardown(data) {
  // Get final metrics
  const metricsRes = http.get(`${BASE_URL}/api/v1/metrics`);
  console.log('=== FINAL METRICS ===');
  console.log(metricsRes.body);

  // Cancel topology
  http.post(`${BASE_URL}/api/v1/topologies/${data.topologyId}/cancel`);
  console.log('Topology canceled');
}

export function handleSummary(data) {
  return {
    'throughput_summary.json': JSON.stringify(data, null, 2),
    stdout: `
=== THROUGHPUT TEST SUMMARY ===
Events Processed: ${data.metrics.events_processed.values.count}
Avg Batch Latency: ${data.metrics.batch_latency_ms.values.avg.toFixed(2)}ms
P95 Batch Latency: ${data.metrics.batch_latency_ms.values['p(95)'].toFixed(2)}ms
P99 Batch Latency: ${data.metrics.batch_latency_ms.values['p(99)'].toFixed(2)}ms
Throughput: ${(data.metrics.events_processed.values.count / 60).toFixed(2)} events/s
`,
  };
}