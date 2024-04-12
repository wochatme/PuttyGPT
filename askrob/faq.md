# A Frequently Asked Questions (FAQ) List

### What is PuttyGPT?

If you know what Putty is, then you know what PuttyGPT is. Putty is one of the most popular SSH tools used wildly in the world. Many IT professionals use Putty everyday during their work. PuttyGPT brings AI chat ability to Putty. That is: when you have technical questions during the operation in Putty window, you can simply click a button to ask AI to help you.

### Is PuttyGPT Open-Sourced?

Absolutely! The development of PuttyGPT is based on the mature codebase of Putty. PuttyGPT does nothing on the kernel of Putty. PuttyGPT only adds the code to communicate with the AI server. You can visit PuttyGPT’s repository on github: https://github.com/wochatme/puttyGPT

### Is PuttyGPT Free?

Because the Gemini Pro from Google is free, so the basic chat service of PuttyGPT is based on Gemini Pro and it is free. But there are some limitations for the basic chat service. These limitations are not from us, but from Google. Please refer the document of Google Gemini for more details.

### What is "conf.json"?

conf.json is the configuration file of PuttyGPT. It is a text file with json format. It is located in the same directory as "puttygpt.exe". When "puttygpt.exe" is launched, it will check if conf.json is exsiting. If not, conf.json with default parameters will be created. After you edit the parameters in conf.json, you have to restart "puttygpt.exe" to make the change effective.

### Does PuttyGPT Gather My Sensitive Data?

By default, when you ask the question by using PuttyGPT, the current output of your Putty window will be sent to AI as well. By doing this, PuttyGPT is much helpful when you have technical trouble in your Putty windows. Because after AI gets the output of your Putty screen, it will know better about your problem and help you more.

If your Putty window contains sensitive data that you do not want to share with AI, there are three options:

- In conf.json file there is a parameter called “screen”. If you set this parameter to 0, then your screen data will not be sent to AI at all.
- When you ask question, add "--"(two minus signs) at the beginning of your question, then the screen data will not be sent to AI.
- PuttyGPT only sends the data you can see from the Putty window, so you can scroll up or scroll down to move your sensitive data out of Putty window. Or you can run some screen clearance command such as "clear" on Linux/MacOS or "cls" on DOS command line to clear the screen before you ask the question.

### How To Use "conf.json"?

conf.json is a pure text-format file and here is the default content of it:
```json
{
"key" : "03339A1C8FDB6AFF46845E49D120E0400021E161B6341858585C2E25CA3D9C01CA",
"url" : "https://www.wochat.org/v1",
"font0" : "Courier New",
"font1" : "Courier New",
"fsize0" : 11,
"fsize1" : 11,
"height" : 140,
"startchat" : 1,
"screen" : 1,
"autologging" : 1,
"proxy_type" : 0,
"proxy" : ""
}
```

The meaning for each parameter is described in the below:
- key : this is a 66-byte long string containing only 0 to 9 and A to Z. AI will use this string to identify who you are and what service it can provide to you.
- url : the web interface where your request to be sent to by using HTTPS. Typically you do not need to change it.
- font0 : the font name of the upper window in the AI chat window. You can use any valid font name such as "Arial", "Terminal", etc. The default value is "Courier New".
- font1 : the font name of the lower window in the AI chat window. That is where you input the text.
- fsize0 : the font size of font0. The default is 11. A typical value range is from 9 to 32 or more.
- fsize1 : the font size of font1. The default is 11. A typical value range is from 9 to 32 or more.
- height : the height of the the lower window in the AI chat window, in pixel.
- startchat : start the AI chat window when the Putty window is created if this value is non-zero. Zero will prevent the creation of AI chat window.
- screen : If this value is non-zero, the output of your Putty window will be sent to AI when you raise question. Zero will prevent this behavior.
- autologging : If this value is non-zero, a log_xxxx.txt will created for each Putty session. All your detailed chat history will be saved into this log file. Zero will prevent this behavior.
- proxy_type : If this value is zero, your connection to AI has no proxy. This is the common case today. But if you use proxy to connect to the internet, you need to set the value of this parameter to non-zero and set the value of "proxy" as well. Please refer the next question.
- proxy : set this parameter when you are using proxy to connect to the internet. Please refer the next question.

### How To Set Proxy
If you are using proxy to connect to the internet, you need to set the parameters of "proxy_type" and "proxy" in conf.json. In the source code, PuttyGPT use the two functions in the below to set the proxy:
```
curl_easy_setopt(curl, CURLOPT_PROXYTYPE, pxtype);
curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
```
So, please refer the document of libCurl for details. In the code, the value of proxy_type is configured like this:
```
            long pxtype = CURLPROXY_HTTP;
            switch (g_proxy_type)
            {
            case 1:
                pxtype = CURLPROXY_HTTP;
                break;
            case 2:
                pxtype = CURLPROXY_HTTP_1_0;
                break;
            case 3:
                pxtype = CURLPROXY_HTTPS;
                break;
            case 4:
                pxtype = CURLPROXY_HTTPS2;
                break;
            case 5:
                pxtype = CURLPROXY_SOCKS4;
                break;
            case 6:
                pxtype = CURLPROXY_SOCKS5;
                break;
            case 7:
                pxtype = CURLPROXY_SOCKS4A;
                break;
            case 8:
                pxtype = CURLPROXY_SOCKS5_HOSTNAME;
                break;
            default:
                break;
            }
```

Please check the libCurl document for more details about the parameters like "CURLPROXY_HTTP" or "CURLPROXY_SOCKS5".

For better understanding for proxy settings, please check the below link as a good start point:

https://curl.se/libcurl/c/CURLOPT_PROXYTYPE.html


### How To Make a New Line in Input Window?
When you hit the ENTER key, your question will be sent to AI. You can use "Ctrl + Enter" to make a new line. That is: hold the CTRL key when you press ENTER key.


### How to Compile PuttyGPT Source Code?

Please check the building document in github: https://github.com/wochatme/PuttyGPT/blob/main/askrob/build.md

This document is greenhand friendly. It provides the detailed step-by-step instructions about how to compile PuttyGPT from the source code or even from the codebase of Putty that you trust. Please try it.

### How to Contact You?

Please send any suggestion or feedback to support@wochat.ai


