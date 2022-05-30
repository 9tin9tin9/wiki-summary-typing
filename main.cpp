#include <ncurses.h>
#include <string>
#include <vector>
#include <algorithm>
#include <time.h>
#include "cJSON.h"

#define KEY_ESCAPE 27
#define nsec(t) (t.tv_sec * 1e9 + t.tv_nsec)
#define WRONG_LETTERS_GROWTH_FACTOR 20
#define CURL_COMMAND "curl https://en.wikipedia.org/api/rest_v1/page/random/summary -L -s"
#define UNIDECODE_COMMAND "unidecode -c "
const size_t ROW = 24, COL = 80;

// TODO: convert unicode to closet ascii
typedef struct{
    std::string text;
    size_t nextPos;
    size_t wordCount;
    std::vector<size_t> wrongLetters;
}Passage;

// https://en.wikipedia.org/api/rest_v1/page/random/summary
// fetch random summary from wiki using the rest api
std::string fetchJson()
{
    printf("Fetching summary from wiki...\n");
    FILE* curl = popen(CURL_COMMAND, "r");
    if (!curl) {
        return "Curl is needed to fetch page from wiki";
    }
    std::string buff;
    char mbuff[1001] = {0};
    size_t size;
    while((size = fread(mbuff, 1, 1000, curl))){
        if (size < 0){
            return strerror(size);
        }
        buff += mbuff;
        memset(mbuff, 0, 1000);
    }
    pclose(curl);
    return buff;
}

std::string parseJson(std::string jsonString)
{
    cJSON* json = cJSON_Parse(jsonString.c_str());
    if (json == NULL){
        return std::string("Failed parsing json: ") + cJSON_GetErrorPtr();
    }
    std::string summary = cJSON_GetObjectItem(json, "extract")->valuestring;

    // print title and website of current page
    printf("Title: %s\n", cJSON_GetObjectItem(json, "title")->valuestring);
    cJSON* content_urls = cJSON_GetObjectItem(json, "content_urls");
    cJSON* desktop = cJSON_GetObjectItem(content_urls, "desktop");
    cJSON* page = cJSON_GetObjectItem(desktop, "page");
    printf("Page: %s\n", page->valuestring);
    putc('\n', stdout);
    cJSON_Delete(json);
    return summary;
}

// convert long characters to ascii using unidecode
std::string unicodeToAscii(std::string summary)
{
    for (auto it = summary.begin(); it < summary.end(); it++){
        if (*it == '"'){
            summary.insert(it, '\\');
            it++;
        }
    }
    std::string command = UNIDECODE_COMMAND + std::string("\"") + summary + "\"";
    FILE* unidecode = popen(command.c_str(), "r");
    if (!unidecode) {
        return "Unidecode is needed to convert unicode to the closest ASCII. Install with 'pip install unidecode'";
    }

    std::string buff;
    char mbuff[1001] = {0};
    size_t size;
    while((size = fread(mbuff, 1, 1000, unidecode))){
        if (size < 0){
            return "Failed converting unicode to ascii using unidecode";
        }
        buff += mbuff;
        memset(mbuff, 0, 1000);
    }
    pclose(unidecode);
    return buff;
}

void appendAndCountWords(Passage* passage, std::string text)
{
    passage->text += text;

    // Word count = (number of white spaces + 1)
    for (auto c : text){
        if (c == ' '){
            passage->wordCount++;
        }
    }
    passage->wordCount++;
}

std::string fetchPassage(Passage* passage)
{
    passage->nextPos = 0;
    passage->wordCount = 0;

    while(1){
        std::string jsonString = fetchJson();
        std::string summary = parseJson(jsonString);
        std::string text = unicodeToAscii(summary);
        appendAndCountWords(passage, text);

        if (passage->wordCount > 100) {
            break;
        }
        std::string padding = std::string(COL-(passage->text.length()%COL), '\n');
        passage->text.insert(passage->text.end(), padding.begin(), padding.end());
    }
    return "";
}

int compar(const void* key, const void* member)
{
    return *(int*)key - *(int*)member;
}

// current line always be the first line printed
// calculate row number of nextPos
// fill the rest of the screen starting from the first letter of the current line
// UNDERLINE the current letter <-- use mouse instead? or highlight background
// STANDOUT wrong letters <-- red background to highlight?
// BOLD typed letters <-- actually hard to see the difference
void drawScreen(const Passage* passage)
{
    clear();
    size_t linenum = passage->nextPos / COL;
    size_t i = linenum * COL;
    size_t w = 0;
    for (size_t y = 0; y < ROW && i < passage->text.length(); y++){
        for (size_t x = 0; x < COL && i < passage->text.length(); x++, i++){
            if (passage->text.at(i) == '\n'){
                continue;
            }
            if (i < passage->nextPos){
                // check for wrong letters
                bool isWrongLetter = 
                    std::binary_search(
                        passage->wrongLetters.begin(),
                        passage->wrongLetters.end(),
                        i);
                if (isWrongLetter){
                    attron(A_STANDOUT);
                }else{
                    attron(A_BOLD);
                }
            }else if (i == passage->nextPos){
                attron(A_UNDERLINE);
            }
            
            mvaddch(y, x, passage->text.at(i));

            attrset(0);
        }
    }
enddrawing:
    refresh();
}

int main()
{
    Passage passage;
    std::string errorStr = fetchPassage(&passage);
    if (errorStr.length()){
        return 1;
    }

    initscr();
    noecho();

    struct timespec start_ts;
    int started = 0;

    while(passage.nextPos < passage.text.length()){
        drawScreen(&passage);

        int c = getch();
        if (c == -1){
            continue;
        }else if (c == KEY_BACKSPACE || c == 127) {
            passage.nextPos -= (passage.nextPos > 0);
            while (passage.text.at(passage.nextPos) == '\n'){
                passage.nextPos--;
            }
            // pop wrong letters if deleted
            while (passage.wrongLetters.size() && passage.wrongLetters.back() == passage.nextPos){
                passage.wrongLetters.pop_back();
            }
            continue;
        }
        
        // start timer
        if (started == 0){
            started = 1;
            timespec_get(&start_ts, TIME_UTC);
        }

        if (c != passage.text.at(passage.nextPos)){
            passage.wrongLetters.push_back(passage.nextPos);
        }
        passage.nextPos++;

        try{
            while (passage.text.at(passage.nextPos) == '\n'){
                passage.nextPos++;
            }
        }catch(...){
            break;
        }
    }

    struct timespec end_ts;
    timespec_get(&end_ts, TIME_UTC);
    double timediff = nsec(end_ts) - nsec(start_ts);

    endwin();

    if (started){
        printf("WPM = %.1f\n", passage.wordCount / (timediff / 1e9 / 60));
        printf("Accuracy = %.1f\n", 100 - passage.wrongLetters.size()  * 100.0 / passage.text.length());
    }

    return 0;
}
