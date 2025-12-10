# CS 114 - Network Security

## Course Overview
This course examines vulnerabilities, attacks, and mitigations at all layers of the network stack, covering cryptography, authentication protocols, botnets, firewalls, intrusion detection systems, and communication privacy/anonymity.

## Key Topics
- **Cryptography**: Public and private key cryptography fundamentals
- **Security Protocols**: Confidentiality and authentication mechanisms
- **Network Threats**: Botnets and distributed attack systems
- **Defense Mechanisms**: Firewalls and intrusion detection systems (IDS)
- **Privacy**: Communication privacy and anonymity techniques
- **Web Security**: Cross-site scripting (XSS), CSRF, cookie manipulation, path traversal, denial of service, code execution vulnerabilities

## Assignments/Projects
The course includes hands-on web exploitation work through the **Gruyere Codelab** (a Google-developed vulnerable web application):

### Web Exploitation Challenges (Bonus Points)
- **Cross-Site Scripting (XSS)**: File upload XSS, reflected XSS, stored XSS via HTML attributes and AJAX
- **Client-State Manipulation**: Elevation of privilege, cookie manipulation
- **Cross-Site Request Forgery (XSRF)**: Exploiting trust between browser and server
- **Cross-Site Script Inclusion (XSSI)**: Including malicious scripts across origins
- **Path Traversal**: Information disclosure and data tampering via directory traversal
- **Denial of Service**: Server quit attacks, resource overloading
- **Remote Code Execution**: Exploiting server-side code vulnerabilities
- **Configuration Vulnerabilities**: Information disclosure through misconfigurations
- **AJAX Vulnerabilities**: DoS and phishing via asynchronous requests

### Assignment Format
Students practice both **black-box hacking** (experimenting without source code access) and **white-box hacking** (analyzing Python source code). Each challenge is worth 1 bonus point toward the final grade. Submissions require a writeup with reproducible exploit steps.

### Tools Mentioned
- Browser developer tools for HTTP header inspection
- Web proxies: Burp Suite, OWASP ZAP

## Additional Topics Covered
- Buffer overflow and integer overflow vulnerabilities
- SQL injection attacks
- Defensive coding practices and vulnerability remediation

## Prerequisites
Not explicitly stated, but assumes familiarity with:
- Basic networking concepts
- Web application architecture
- Python (helpful but not required for exploitation exercises)
