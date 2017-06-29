#include <unistd.h>
#include <stdio.h>
#include <curl/curl.h>

void saveData(void *ptr, size_t size, size_t nmemb, void *userdata)  
{  
    sprintf((char *)userdata,"%s",(char *)ptr);  
}

int main() {
    CURL *curl;
    CURLcode res;

    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
        just as well be a https:// URL if that is what should receive the
        data. */
        curl_easy_setopt(curl, CURLOPT_URL, "127.0.0.1");
        
        // 设置http发送的内容类型为JSON
        curl_slist *plist = curl_slist_append(NULL,
                "Content-Type:application/json;charset=UTF-8");
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, plist);
        
        /* Now specify the POST data */
        char * data="{\"user_name\" : \"test\",\"password\" : \"test123\"}";
 
        
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        
        char receive[1024]; 
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, receive);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, saveData);
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
        printf("client recv %s\n",receive);
    }
    
    curl_global_cleanup();
    return 0;
}



