# LLM MITM Proxy

HTTPS proxy that injects an AI chat widget into the Tufts SIS web page.

## Run

```bash
docker pull bixvongoeler/llm_mitmproxy_containerized
docker run -d -p 9999:9999 -p 5001:5001 \
    -e LLMPROXY_API_KEY=your-key \
    -e LLMPROXY_ENDPOINT=your-endpoint \
    bixvongoeler/llm_mitmproxy_containerized
```

## Browser Setup

1. Set HTTP proxy to `localhost:9999`
2. Install `crt/proxy_ca.crt` as trusted CA

## Environment Variables

| Variable            | Required |
| ------------------- | -------- |
| `LLMPROXY_API_KEY`  | Yes      |
| `LLMPROXY_ENDPOINT` | Yes      |
