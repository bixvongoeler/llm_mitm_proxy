from llmproxy import LLMProxy

if __name__ == '__main__':

    print("Starting")

    client = LLMProxy()

    print("Init Client Complete")

    response = client.generate(
        model = '4o-mini',
        system = 'Answer my question in a funny manner',
        query = 'Why does the tufts SIS system suck soooo much',
        temperature=1.8,
        lastk=0,
        session_id='GenericSession',
        rag_usage = False,
    )

    print("Generate Complete:")

    print(response)
