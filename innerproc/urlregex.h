#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

/* The following is the size of a buffer to contain any error messages
   encountered when the regular expression is compiled. */

#define MAX_ERROR_MSG 0x1000

/* Compile the regular expression described by "regex_text" into
   "r". */

static int compile_regex (regex_t * r, const char * regex_text)
{
    int status = regcomp (r, regex_text, REG_EXTENDED|REG_NEWLINE);
    if (status != 0) {
	char error_message[MAX_ERROR_MSG];
	regerror (status, r, error_message, MAX_ERROR_MSG);
        printf ("Regex error compiling '%s': %s\n",
                 regex_text, error_message);
        return 1;
    }
    return 0;
}

/*
  Match the string in "to_match" against the compiled regular
  expression in "r".
 */

static int extract_string_by_match_regex (regex_t * r, const char * to_match,char * matchedchar)
{
    /* "P" is a pointer into the string which points to the end of the
       previous match. */
    const char * p = to_match;
    /* "N_matches" is the maximum number of matches allowed. */
    const int n_matches = 1;//only one we need
    /* "M" contains the matches found. */
    regmatch_t m[n_matches];

    while (1) {
        int i = 0;
        int nomatch = regexec (r, p, n_matches, m, 0);
        if (nomatch) {
            printf ("No more matches.\n");
            return nomatch;
        }
        for (i = 0; i < n_matches; i++) {
            int start;
            int finish;
            if (m[i].rm_so == -1) {
                break;
            }
            start = m[i].rm_so + (p - to_match);
            finish = m[i].rm_eo + (p - to_match);
            if (i == 0) {
		strncpy(matchedchar,to_match+start,finish-start);			

//               printf ("$& is ");
            }
/*
            else {
                printf ("$%d is ", i);
            }
            printf ("'%.*s' (bytes %d:%d)\n", (finish - start),
                    to_match + start, start, finish);
*/
        }
        p += m[0].rm_eo;
    }
    return 0;
}

static int replace_string_by_match_regex (regex_t * r, const char * to_match,char * newsubstr, char * newstring )
{
    /* "P" is a pointer into the string which points to the end of the
       previous match. */
    const char * p = to_match;
    /* "N_matches" is the maximum number of matches allowed. */
    const int n_matches = 1;//only one we need
    /* "M" contains the matches found. */
    regmatch_t m[n_matches];

    while (1) {
        int i = 0;
        int nomatch = regexec (r, p, n_matches, m, 0);
        if (nomatch) {
            //printf ("No more matches.\n");
            return nomatch;
        }
        for (i = 0; i < n_matches; i++) {
            int start;
            int finish;
            if (m[i].rm_so == -1) {
                break;
            }
            start = m[i].rm_so + (p - to_match);
            finish = m[i].rm_eo + (p - to_match);
            if (i == 0) {
		strncpy(newstring,to_match,start);
		strcat(newstring,newsubstr);
		strcat(newstring,to_match+finish);
//                printf ("$& is ");
            }
/*            else {
                printf ("$%d is ", i);
            }
            printf ("'%.*s' (bytes %d:%d)\n", (finish - start),
                    to_match + start, start, finish);
*/
        }
        p += m[0].rm_eo;
    }
    return 0;
}

void url_replace (char *orig_url_string,char *new_url_string,char *ip,int port)
{
    regex_t r;
    const char * regex_text;
    char matchedchar[64];
    bzero(matchedchar,64);
    char newstring[1024];
    bzero(newstring,1024);

    //ip:port regex
    regex_text = "((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]).){3}(25[0-5]|2[0-5][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]):([0-9]+)";

    printf ("Trying to find '%s' in '%s'\n", regex_text, orig_url_string);
    compile_regex (& r, regex_text);
    //extract_string_by_match_regex (& r, orig_url_string,matchedchar);
    printf("matchedchar:%s\n",matchedchar);
    replace_string_by_match_regex(& r,orig_url_string,"192.168.0.199:555",newstring);
    printf("newstring:%s\n",newstring);
    regfree (& r);
}

//extract ip string from url 
void extract_ip_by_regex(char *orig_url_string,char *ip)
{
    regex_t r;
    const char * regex_text;

    regex_text = "((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]).){3}(25[0-5]|2[0-5][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])";
    compile_regex (& r, regex_text);
    extract_string_by_match_regex (& r, orig_url_string,ip);
}


//extract port from url 

int extract_port_by_regex(char *orig_url_string)
{
    regex_t r;
    const char * regex_text;
    char portstr[8];

    regex_text = ":([0-9]+)";
    compile_regex (& r, regex_text);
    extract_string_by_match_regex (& r, orig_url_string,portstr);

    //trim ':'
    return atoi(portstr+1);
}
