import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

const BASE_URL = __ENV.MASTER_URL || 'http://127.0.0.1:8080';

// Custom metrics
const topologySubmitRate = new Rate('topology_submit_success');
const ingestRate = new Rate('ingest_success');
const ingestLatency = new Trend('ingest_latency_ms');

export const options = {
  stages: [
    { duration: '30s', target: 10 },  // Ramp up to 10 users
    { duration: '1m', target: 10 },   // Stay at 10 users
    { duration: '30s', target: 20 },  // Ramp to 20 users
    { duration: '1m', target: 20 },   // Stay at 20
    { duration: '30s', target: 0 },   // Ramp down
  ],
  thresholds: {
    http_req_duration: ['p(95)<500'], // 95% of requests under 500ms
    topology_submit_success: ['rate>0.9'], // 90% success rate
    ingest_success: ['rate>0.95'], // 95% success rate
  },
};

// Test topology spec
const testTopology = {
  name: 'load-test-topology',
  description: 'k6 load test',
  operators: [
    {
      id: 'map1',
      kind: {
        type: 'map',
        field: 'message',
        transform: { kind: 'to_lower' }
      },
      config: {}
    },
    {
      id: 'filter1',
      kind: {
        type: 'filter',
        field: 'level',
        predicate: {
          kind: 'equals',
          value: 'error'
        }
      },
      config: {}
    },
    {
      id: 'sink',
      kind: { type: 'sink_log' },
      config: {}
    }
  ],
  edges: [
    { from: 'map1', to: 'filter1' },
    { from: 'filter1', to: 'sink' }
  ],
  parallelism: 1
};

export function setup() {
  // Check master is alive
  const res = http.get(`${BASE_URL}/api/v1/metrics`);
  check(res, {
    'master is alive': (r) => r.status === 200,
  });
  
  return { topologyIds: [] };
}

export default function(data) {
  // Submit topology
  const submitRes = http.post(
    `${BASE_URL}/api/v1/topologies`,
    JSON.stringify(testTopology),
    { headers: { 'Content-Type': 'application/json' } }
  );

  const submitSuccess = check(submitRes, {
    'topology submit status 200': (r) => r.status === 200,
    'topology has id': (r) => {
      try {
        const body = JSON.parse(r.body);
        return body.topology_id !== undefined;
      } catch (e) {
        return false;
      }
    }
  });

  topologySubmitRate.add(submitSuccess);

  if (!submitSuccess) {
    console.error(`Submit failed: ${submitRes.status} ${submitRes.body}`);
    sleep(1);
    return;
  }

  const topologyId = JSON.parse(submitRes.body).topology_id;
  
  // Wait for deployment
  sleep(2);

  // Ingest events
  const events = [];
  for (let i = 0; i < 10; i++) {
    events.push({
      timestamp: new Date().toISOString(),
      data: {
        level: i % 2 === 0 ? 'ERROR' : 'INFO',
        message: `Test message ${i}`,
        service: 'api',
        request_id: `req-${i}`
      }
    });
  }

  const ingestPayload = {
    topology_id: topologyId,
    events: events
  };

  const startTime = Date.now();
  const ingestRes = http.post(
    `${BASE_URL}/api/v1/ingest`,
    JSON.stringify(ingestPayload),
    { headers: { 'Content-Type': 'application/json' } }
  );
  const endTime = Date.now();

  const ingestSuccess = check(ingestRes, {
    'ingest status 202': (r) => r.status === 202,
  });

  ingestRate.add(ingestSuccess);
  ingestLatency.add(endTime - startTime);

  // Check topology status
  const statusRes = http.get(`${BASE_URL}/api/v1/topologies/${topologyId}`);
  check(statusRes, {
    'status check success': (r) => r.status === 200,
  });

  sleep(1);

  // Cancel topology
  http.post(`${BASE_URL}/api/v1/topologies/${topologyId}/cancel`);
  
  sleep(1);
}

export function teardown(data) {
  // Get final metrics
  const metricsRes = http.get(`${BASE_URL}/api/v1/metrics`);
  console.log('Final metrics:', metricsRes.body);
}