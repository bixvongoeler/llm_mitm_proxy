# LLM MITM Proxy
*HTTPS proxy that injects an AI chat widget into the Tufts SIS web page*

Image available on [Docker Repo](https://hub.docker.com/r/bixvongoeler/llm_mitmproxy_containerized)

## Run

```bash
docker pull bixvongoeler/llm_mitmproxy_containerized
docker run -d -p 9999:9999 -p 5001:5001 \
    -e LLMPROXY_API_KEY=your-key \
    -e LLMPROXY_ENDPOINT=your-endpoint \
    bixvongoeler/llm_mitmproxy_containerized
```

## Browser Setup and Usage

1. Set HTTP proxy to `localhost:9999`
2. Install `crt/proxy_ca.crt` as trusted CA (can download from https://github.com/bixvongoeler/llm_mitmproxy_containerized)
3. Navigate to SIS Course Search Page (https://sis.it.tufts.edu/psp/paprd/EMPLOYEE/EMPL/h/?tab=TFP_CLASS_SEARCH#class_search)
4. You may need to perform a force refresh (cmd+shift+r)
5. Click the chat widget in the bottom corner
6. Make a course search (LLM has info about CS courses only)
7. Upload a transcript for personal context
8. Chat with the SIS Academic Advisor!

## Environment Variables

| Variable            | Required |
| ------------------- | -------- |
| `LLMPROXY_API_KEY`  | Yes      |
| `LLMPROXY_ENDPOINT` | Yes      |
