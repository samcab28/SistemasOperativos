// Perfil de Carga MEDIA - 50 VUs por 60s
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 50,
  duration: '60s',
  thresholds: {
    'http_req_duration': ['p(95)<2000', 'p(99)<5000'],
    'http_req_failed': ['rate<0.05'],
    'checks': ['rate>0.95'],
  },
};

const BASE = 'http://localhost:8080';

const MEDIUM_ROUTES = [
  '/reverse?text=mediumload',
  '/toupper?text=performance',
  '/hash?text=loadtest',
  '/fibonacci?num=30',
  '/random?count=500&min=1&max=1000',
  '/timestamp',
  '/isprime?n=982451653&algo=mr',
  '/factor?n=360360',
  '/wordcount?name=grep.txt',
  '/grep?name=grep.txt&pattern=test',
];

export default function () {
  const route = MEDIUM_ROUTES[Math.floor(Math.random() * MEDIUM_ROUTES.length)];
  const res = http.get(`${BASE}${route}`);
  
  check(res, {
    'status 200': r => r.status === 200,
    'response time < 5s': r => r.timings.duration < 5000,
  });
  
  sleep(0.2);
}

export function handleSummary(data) {
  const reqs = data.metrics.http_reqs.values.count;
  const rate = data.metrics.http_reqs.values.rate;
  const p50 = data.metrics.http_req_duration.values['p(50)'];
  const p95 = data.metrics.http_req_duration.values['p(95)'];
  const p99 = data.metrics.http_req_duration.values['p(99)'];
  const failed = data.metrics.http_req_failed.values.rate * 100;
  
  console.log('\n========================================');
  console.log('  LOAD TEST - MEDIUM PROFILE');
  console.log('========================================');
  console.log(`Profile: 50 VUs × 60s`);
  console.log(`Total requests: ${reqs}`);
  console.log(`Throughput: ${rate.toFixed(2)} req/s`);
  console.log('\nLatencies:');
  console.log(`  p50: ${p50.toFixed(2)}ms`);
  console.log(`  p95: ${p95.toFixed(2)}ms`);
  console.log(`  p99: ${p99.toFixed(2)}ms`);
  console.log(`\nError rate: ${failed.toFixed(2)}%`);
  console.log('========================================\n');
  
  return { 'stdout': '' };
}