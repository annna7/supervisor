#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
   
/*     if (argc != 2) {
        fprintf(stderr, "Usage: %s <message> %d\n", argv[0], argc);
        return 1;
    } */
    printf("%d %s %s\n", argc, argv[0], argv[1]);  
    const char *message = argv[1];
    FILE *log_file;
    log_file = fopen("/home/anna/Desktop/so/supervisor/build/bin/log.txt", "w");
    if (log_file == NULL) {
	perror("error open file");
	return 1;
    }
    for (int i = 0; i < 100; ++i) {
	    fprintf(log_file, "%d, %s\n", i, message);
	    fflush(log_file);
	    sleep(2);
    }
    fclose(log_file);
   // int i = 0;
    //while (1) {
    //    printf("%s %d\n", message, i);
     //   ++i;
       // sleep(1);
   // }
   return 0;
}
