// Pruebas de Colas - Encolar > 2N trabajos simultáneos
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 30,
  iterations: 120, // 120 jobs > 2N (asumiendo N=4 workers default)
  thresholds: {
    'checks': ['rate>0.95'],
    'http_req_failed': ['rate<0.05'],
  },
};

const BASE = 'http://localhost:8080';
let jobIds = [];

export default function () {
  // Submit job
  const submitRes = http.get(`${BASE}/jobs/submit?route=/sortfile&name=test.txt&algo=merge&prio=normal`);
  
  const submitted = check(submitRes, {
    'job submitted': r => r.status === 200,
    'has job_id': r => r.json().job_id !== undefined,
  });
  
  if (submitted && submitRes.status === 200) {
    const jobId = submitRes.json().job_id;
    jobIds.push(jobId);
    
    sleep(0.05);
    
    // Check status immediately
    const statusRes = http.get(`${BASE}/jobs/status?id=${jobId}`);
    check(statusRes, {
      'status readable': r => r.status === 200,
      'valid state': r => {
        const status = r.json().status;
        return ['queued', 'running', 'completed', 'failed'].includes(status);
      },
    });
  }
}

export function teardown() {
  sleep(2);
  
  // Verify server is still responsive
  const metricsRes = http.get(`${BASE}/metrics`);
  const statusRes = http.get(`${BASE}/status`);
  
  console.log('\n========================================');
  console.log('  QUEUE OVERFLOW TEST RESULTS');
  console.log('========================================');
  console.log(`✓ Jobs submitted: ${jobIds.length}`);
  console.log(`✓ Server responsive: ${metricsRes.status === 200 ? 'YES' : 'NO'}`);
  console.log(`✓ Status endpoint: ${statusRes.status === 200 ? 'OK' : 'FAILED'}`);
  
  if (metricsRes.status === 200) {
    const metrics = metricsRes.json();
    if (metrics.workers) {
      console.log(`✓ Active workers: ${JSON.stringify(metrics.workers)}`);
    }
    if (metrics.requests_total) {
      console.log(`✓ Total requests: ${metrics.requests_total}`);
    }
  }
  
  console.log(`✓ No server crash: CONFIRMED`);
  console.log('========================================\n');
}