### **BridgeLLM – User Guide**

Contact: [abdullah@cs.tufts.edu](mailto:abdullah@cs.tufts.edu) or [fahad@cs.tufts.edu](mailto:fahad@cs.tufts.edu)  
(If you have any questions, you can also post them as comments to this document)  
You can also use this [ChatGPT assistant](https://chatgpt.com/g/g-67c1cef0e19081919605ed2f0bd85376-llmbridge-api-assistant) for questions regarding the proxy

The NAT lab at Tufts has built an LLM proxy that can make it easier (and cost-effective) for applications to access various LLMs (e.g., openAI, Claude, other models). It has various features like context management, caching, model selection (please see the following [paper](https://arxiv.org/abs/2410.11857) if you are interested in the design of the proxy.)

We have exposed a subset of the proxy’s functionality to external users like you through a simple Python-based API. You can access it by downloading the following [code](https://github.com/Tufts-University/LLMProxy) \[[https://github.com/Tufts-University/LLMProxy](https://github.com/Tufts-University/LLMProxy)\] from github (you will need your Tufts credentials to access this repository; please use this [link](https://github.com/orgs/Tufts-University/sso) to login through the Tufts SSO). The code also contains simple examples and a README.

We currently support a few models, context management (so your application doesn’t have to remember the conversation history), and have support for retrieval augmented generation (RAG).

**(API access key.** You will need an access key to authenticate the requests made to LLMProxy. Please contact us if you don’t have a key )

**1\. generate() function:**

Here is a sample code that shows how you can use the API to make calls to different LLMs.

response \= generate(model \= '4o-mini',

        system \= 'Answer my question in a funny manner',

        query \= 'Who are the Jumbos?',

        temperature=0.0,

        lastk=0,

        session\_id='GenericSession'

        rag\_usage \= True,

        rag\_threshold \= 0.5,

        rag\_k \= 1\)

The generate function **requires** the following parameters:

1. `model (str):` Specifies which LLM to use. Different models offer different capabilities. The following models are included by default in all plans: [OpenAI GPT4o-mini](https://openai.com/index/gpt-4o-mini-advancing-cost-efficient-intelligence/), [Anthropic Claude Haiku](https://www.anthropic.com/news/claude-3-haiku), and [Microsoft Phi3](https://azure.microsoft.com/en-us/blog/introducing-phi-3-redefining-whats-possible-with-slms/). The model IDs for some of these models are: “`4o-mini”`, “us.`anthropic.claude-3-haiku-20240307-v1:0”`, and `“azure-phi3”.` Additional models may be available depending on your access level. To see the full list of models currently accessible with your API key, use the `model_info()` function. If needed, we can provide access to advanced models (e.g., GPT4o) as well
2. `system (str):` You can use this to provide instructions to the LLM to respond in a certain way to your query (described next)
3. `query (str):` What the LLM will be responding to, using the instructions provided as guidelines

There are other **optional** parameters that you can specify:

1. Optional parameters to configure context/randomness in output:

1. `temperature (float):` Read more about it [here](https://www.coltsteele.com/tips/understanding-openai-s-temperature-parameter). The acceptable range is between 0 and 2\. If not specified, a default of 0.7 is used
1. `lastk (int):` The number of previous request-response pairs you want to use as additional context. The LLMProxy maintains a history of requests and responses in chronological order on a per session ID (described next) basis. You can specify a value \>= 0 denoting the amount of previous context you want to attach with the current request. If not specified a default value of 0 (no context) is used
1. `session_id (str):` Specifies the session identifier for maintaining multiple contexts in LLMProxy. This is particularly useful when used with `lastk` to track conversation history.
    1. If not provided, the default session ID `"GenericSession"` is used, grouping all request-response pairs under it
    2. Session IDs **should not** contain hyphens (`-`)
    3. They only need to be unique within your application; no need to worry about collisions with other users

        B. Optional parameters to configure Retrieval Augmented Generation (RAG):

1. `rag_usage (bool):`If set to True, RAG will be utilised. If set to False, RAG will not be used. Default value is False.
1. `rag_threshold (float):`The minimum similarity threshold for a chunk to be used for RAG \- value can be equal to or between 0 and 1\. Default value is 0.5.
1. `rag_k (int):` Number of retrieved chunks to use for RAG \- value can be equal to or between 0 and 10\. Default value is 0\.

**Return Value:**  
The generate() function returns a dictionary, containing ‘result’ and ‘rag_context’. The key ‘result’ contains the response to the query and ‘rag_context’ contains the context used, if any, for RAG.

**Request format considerations.** In the code files provided, the request is sent to the LLMProxy as a POST request over a RESTful API. You need to ensure that the request is JSON-decodable. In particular, if the `query`/`system` parameters contain unescaped characters (e.g., those in `<i>`, `<b>`, and `<a>` tags), they may not be JSON-decodable if they contain quotes (`"`) that are not properly escaped.

For example if the `query`/`system` contains something like \<a href="/wiki/Musical_film" title="Musical film"\>, the LLM request will fail. This is because it contains unescaped double quotes ("), which are not allowed in JSON. Double quotes need to be escaped with a backslash (\\"). Please read more about it [here](https://www.json.org/json-en.html)\!

**H[ere](https://docs.google.com/presentation/d/1u_fYQrF6gEmwNjDSPgtgLFC62fmCHWwO/edit?usp=sharing&ouid=115685644109280726201&rtpof=true&sd=true) are slides used to explain the RAG API.**

**2\. retrieve() function:**  
This function fetches relevant context based on a given `query` and `session_id`, allowing for RAG context reuse across different sessions. It filters results using similarity (`rag_threshold`) and quantity (`rag_k`), returning document summaries and relevant text chunks.

Here is a sample code showing the API usage:  
response \= retrieve(  
 query \= 'Tell me about Orange Jim?',  
 session_id='GenericSession',  
 rag_threshold \= 0.5,  
 rag_k \= 1\)

**Parameters.** The parameters are a subset of those described under generate().

**Return Value:**  
The return value is a list of dictionaries, each representing a document with relevant context. Each dictionary contains a `doc_id`, an LLM-generated `summary`, and a list of `chunks` from the document matching the query.

The retrieve() function is useful if you have some global files that apply to all users or you want to first verify the chunks before passing them on to the generate() call. Typically when retrieve() is used, you wouldn't use the rag functionality in the generate() call because you are explicitly passing the chunks.

**3\. pdf_upload() function:**

Here is sample code that shows how you can use the API to upload a PDF (file size limit: **4MB**) for RAG:

pdf_upload(path \= 'greentim.pdf',

        session\_id \= 'GenericSession',

        strategy \= 'smart')

1. `path (str):`Path to the PDF you want to upload.
2. `session_id (str):` The uploaded PDF will be available for RAG in the session with this `session_id`.
3. `strategy (str):` The chunking strategy used to split documents into retrievable chunks. For now, it should be set to `‘smart’`.

### **4\. On Query and System**

The `system` and `query` need to be designed based on your usage. For example, in order to implement a web page summarizer, you could set `system` to something like:  
`#### Instructions####`  
`Your task is to summarize the content of web pages provided.`  
`You must strictly follow these:`

1. `Focus on the main ideas, important details, and any actionable insights`
2. `Ensure the summary is concise`
3. `Exclude advertisements, navigation text, or unrelated content`

while the `query` will contain the actual content of the web page (with unescaped characters handled). Having well crafted instructions will tend to result in higher quality responses. Read more about it [here](https://platform.openai.com/docs/guides/prompt-engineering)\!

### **5\. Usage limits**

**Quota.** There is a quota associated with each key (512 requests/day, 10 requests/sec). Please stay within the usage limit. If you feel you need a higher quota, please reach out to us.

**PDF Size**. 4MB is the PDF size limit for RAG. If you need to upload a bigger PDF, you can split it into smaller PDFs and upload those.

**Request Time.** If your requests take longer than 59 seconds, the generate() API will timeout. If you are frequently running into timeouts, please get in touch with us.
