/*
  * gophers.c: gopher server for plan 9 by sean caron (scaron@umich.edu)
*/

#include <u.h>
#include <libc.h>
#include <stdio.h>

/* the FQDN of the host that will be running the gopher server */

#define	HOST_NAME	"primrose.diablonet.net"

int main(int argc, char **argv);
void timestamp(char *stamp);
int dotgopher(int netfd, int logfd, char *rootd);
int readaline(int infd, char **ptr);
int counttabs(char *lin);

int main(int argc, char **argv) {
    int dfd, acfd, lcfd, rfd, qfd, lgfd;
    char adir[40], ldir[40], *line, anc_buf[40];
    int i, n, w, r = 0;
    long ndirs, c;

    Dir *dir;
    char *filetyp;

    char *port, *rootdir, *logfile;

    char tstamp[32];

    char *path, *temp;
    char fbuf[512];

    if (argc != 4) {
        print("usage: %s port rootdir logfile\n", argv[0]);
        exits(0);
    }

    /* create some more descriptive variables for the arguments and allocate memory for them */
    port = malloc(strlen(argv[1])*sizeof(char)+1);

    if (port == 0) {
        exits("MALLOC");
    }

    strcpy(port, argv[1]);

    rootdir = malloc(strlen(argv[2])*sizeof(char)+1);

    if (rootdir == 0) {
        exits("MALLOC");
    }

    strcpy(rootdir, argv[2]);

    logfile = malloc(strlen(argv[3])*sizeof(char)+1);

    if (logfile == 0) {
        exits("MALLOC");
    }

    strcpy(logfile, argv[3]);

    /* remove trailing slash from root directory and logfile if one was included by the user */
    if (rootdir[strlen(rootdir)-1] == '/') {
        rootdir[strlen(rootdir)-1] = '\0';
    }

    if (logfile[strlen(logfile)-1] == '/') {
        logfile[strlen(logfile)-1] = '\0';
    }

    /* if logfile exists append to it, otherwise create it */
    if ((lgfd = open(logfile, OWRITE)) < 0) {	
        if ((lgfd = create(logfile, OWRITE, 0777)) < 0) {
            printf("ERROR: Cannot create logfile; exiting.\n");

            free(logfile);
            free(rootdir);
            free(port);

            exits("CREATE");
        }
    }

    /* if the logfile already exists go to the end of the file, otherwise this has no effect */
    seek(lgfd, 0, 2);

    timestamp(tstamp);
    fprint(lgfd, "%s Starting gopher server on port %s\n", tstamp, port);
    fprint(lgfd, "%s Root directory: %s\n", tstamp, rootdir);
    fprint(lgfd, "%s Logfile: %s\n", tstamp, logfile);

    /* listen on the specified port for gopher clients */
    sprintf(anc_buf, "tcp!*!%s", port);

    acfd = announce(anc_buf, adir);
	
    if (acfd < 0) {
        free(port);
        free(logfile);
        free(rootdir);

        timestamp(tstamp);
        fprint(lgfd, "%s ERROR: announce() failed; exiting.\n", tstamp);
        close(lgfd);

        exits("ANNOUNCE");
    }

    while(1) {
        /* listen for a call */
        lcfd = listen(adir, ldir);
		
        if (lcfd < 0) {
            free(port);
            free(logfile);
            free(rootdir);

            timestamp(tstamp);
            fprint(lgfd, "%s ERROR: listen() failed; exiting.\n", tstamp);
            close(lgfd);

            exits("LISTEN");
        }

        /* fork a process to handle the request */
        switch(fork()) {
            /* fork error */
            case -1:
                close(lcfd);
                break;
				
            /* child */
            case 0:

            /* accept the call and open the data file */
            dfd = accept(lcfd, ldir);

            if (dfd < 0) {
                close(dfd);
                close(lcfd);

                timestamp(tstamp);
                fprint(lgfd, "%s ERROR: accept() failed; child process exiting.\n", tstamp);
                close(lgfd);

                exits("ACCEPT");
            }

            /* read a line from the network */
            readaline(dfd, &line);

            /* if client just sent CR+LF give them the root directory */
            if ((line[0] == '\r') && (line[1] == '\n')) {
                strcpy(line, "");

                path = malloc(strlen(rootdir)*sizeof(char)+1);

                if (path == 0) {
                    exits("MALLOC");
                }

                strcpy(path, rootdir);
            }

            /* otherwise the client actually requested a particular file or directory */

            else {
                /* strip CR+LF from input */
                for (i = 0; i < strlen(line); i++) {
                    if ( line[i] == '\r' ) {
                        line[i] = '\0';
                    }
                }

                /* add our gopher server directory root to get a real file path */
                path = malloc((strlen(rootdir)+strlen(line)+1)*sizeof(char));

                if (path == 0) {
                    exits("MALLOC");
                }

                strcpy(path, strcat(rootdir, line));
            }

            timestamp(tstamp);
            fprint(lgfd, "%s Client requested %s\n", tstamp, path);

            /* figure out if this is a file or a directory */
            dir = dirstat(path);

            /* try to exit gracefully if the file or directory does not exist */
            if (dir == nil) {
                timestamp(tstamp);
                fprint(lgfd, "%s ERROR: Cannot find file or directory %s; child process exiting.\n", tstamp, path);
                close(lgfd);

                close(dfd);
                close(lcfd);

                free(line);
                free(path);

                exits(0);
            }

            /* directory */
            if ((dir->mode & 0x80000000) == 0x80000000) {

            /*
             * first we look for a .gopher file. if in the subsequent
             * routine. if we find one, we basically take it to be raw
             * gopher markup language and give that, and only that,
             * straight to the client.
            */

            r = dotgopher(dfd, lgfd, path);

            /*
             * if there is no .gopher file, we just fall back to a sort of
             * legacy mode, where the server will generate a directory
             * listing and send that to the client.
            */

            if (r != 1) {
                /* try to open the directory and fail gracefully if it doesnt exist */
                rfd = open(path, OREAD);

                if (rfd < 0) {
                    timestamp(tstamp);
                    fprint(lgfd, "%s ERROR: Cannot open directory %s; child process exiting.\n", tstamp, path );
                    close(lgfd);

                    close(dfd);
                    close(lcfd);

                    free(line);
                    free(path);

                    exits(0);
                }

                ndirs = dirreadall(rfd, &dir);

                close(rfd);

                filetyp = malloc((ndirs * sizeof(char)));

                if (filetyp == 0) {
                    exits("MALLOC");
                }

                timestamp(tstamp);
                fprint(lgfd, "%s Read %ld directory entries from %s\n", tstamp, ndirs, path);

                /* classify each entry as either a file or a directory */
                for (c = 0; c < ndirs; c++) {
                    if ((dir[c].mode & 0x80000000) == 0x80000000 ) {
                        filetyp[c] = '1';
                    }

                    else {
                        filetyp[c] = '0';
                    }
                }

                /* give the directory listing to the client, line by line */
                for (c = 0; c < ndirs; c++) {
                    temp = malloc((strlen(line)+strlen(dir[c].name)+10)*sizeof(char));

                    if (temp == 0) {
                        exits("MALLOC");
                    }

                    strcpy(temp, line);

                    if (temp[strlen(temp)-1] != '/') {
                        strcat(temp, "/");
                    }

                    strcat(temp, dir[c].name);

                    if (strcmp(dir[c].name, ".gopher") != 0) {
                        /* for now, hard code the FQDN of the gopher server in the statement below */
                        fprint(dfd, "%c%s\t%s\t%s\t70\r\n", filetyp[c],
                        dir[c].name, temp, HOST_NAME);
                    }

                    free(temp);
                }

                fprint(dfd, "\r\n.\r\n");

                free(filetyp);
            }
        }			

        /* file */
        else {
            /* try to open the file and fail gracefully if it doesnt exist */
            qfd = open(path, OREAD);

            if (qfd < 0) {
                timestamp(tstamp);
                fprint(lgfd, "%s ERROR: Cannot open file %s; child process exiting.\n", tstamp, path);
                close(lgfd);

                close(dfd);
                close(lcfd);

                free(line);
                free(path);

                exits(0);
            }

            else {
                /* read in the file and spit data back to the client */
                while((w = read(qfd, fbuf, sizeof(fbuf))) > 0) {
                    write(dfd, fbuf, w);
                }
            }

            close(qfd);
        }

        close(dfd);
        close(lcfd);

        close(lgfd);

        free(line);
        free(path);

        exits(0);

        /* parent */
        default:
            close(lcfd);
            break;
        }
    }
}

/* generate a timestamp for log file entries */

void timestamp(char *stamp) {
    Tm *ti;

    ti = localtime(time(0));

    sprint(stamp, "%02d/%02d/%04d %02d:%02d:%02d", (ti->mon)+1, ti->mday, (ti->year)+1900,
        ti->hour, ti->min, ti->sec);

    return;
}

int dotgopher(int netfd, int logfd, char *rootd) {
    int n, fd, r = 0;
    char stamp[32], *temp, *line;

    int len = 1;

    /* assemble a path for .gopher and look for it */
    temp = malloc((strlen(rootd)+10)*sizeof(char));

    if (temp == 0) {
        exits("MALLOC");
    }

    strcpy(temp, rootd);

    if (temp[strlen(temp)-1] != '/') {
        strcat(temp, "/");
    }

    strcat(temp, ".gopher");
	
    fd = open(temp, OREAD);

    timestamp(stamp);
    fprint(logfd, "%s Looking for .gopher file %s ", stamp, temp);

    /* if it exists, read it in and dump it to the network */
    if (fd >= 0) {
        fprint(logfd, "-> found.\n");

        /* set result code */
        r = 1;

        line = malloc(sizeof(char));

        while (1) {
            n = read(fd, &(*(line+len-1)), 1);

            line = realloc(line, (len+1)*sizeof(char));

            if (line == 0) {
                exits("REALLOC");
            }

            if (*(line+len-1) == 10) {
                *(line+len-1) = '\0';

                /* 
                 *  if the line looks to be a pre-formed selector just print it out
                 * directly to the client. it is up to the person composing the .gopher
                 * file to make sure these are correct (for now). that is, we expect:
                 *
                 * [id][selector name]\t[path]\t[server fqdn]\t[gopher port]
                 *
                 * the path should have a leading slash in it. i suppose we could correct
                 * for this programmatically in the future.
                */

                if (((line[0] == '0') || (line[0] == '1') || (line[0] == '2') || (line[0] == '3') ||
                    (line[0] == '4') || (line[0] == '5') || (line[0] == '6') || (line[0] == '7') ||
                    (line[0] == '8') || (line[0] == '9') || (line[0] == 'd') || (line[0] == 'g') ||
                    (line[0] == 'h') || (line[0] == 'i' ) || (line[0] == 'l') || (line[0] == 's')) &&
                    (counttabs(line) == 3)) {

                    fprint(netfd, "%s\r\n", line);
                }

                /* otherwise use the de facto i-line to print it as an informational text line */
                else {
                    fprint(netfd, "i%s\t\terror.host\t1\r\n", line);
                }

                /* reset the buffer for the next input line */
                free(line);
                line = malloc(sizeof(char));

                if (line == 0) {
                    exits("MALLOC");
                }

                len = 1;
            }

            else {
                len = len + 1;
            }

            /* eof */
            if (n == 0) {
                break;
            }
        }
    }

    else {
        fprint(logfd, " -> not found.\n");

        /* return code */
        r = 0;

        /*
         * return before hitting the free() statements below since we never called
         * malloc() in this branch.
        */

        return r;
    }

    close(fd);

    free(temp);
    free(line);

    /* return result to caller so it knows if .gopher existed or not */
    return r;
}

/* read in a line from the specified file descriptor to the specified buffer */

int readaline(int infd, char **ptr) {
    int len = 1;
    int n;

    *ptr = malloc(sizeof(char));

    while (1) {
        n = read(infd, &(*(*ptr+len-1)), 1);

        *ptr = realloc(*ptr, (len+1)*sizeof(char));

        if (*ptr == 0) {
            exits("REALLOC");
        }

        if (*(*ptr+len-1) == 10) {
            *(*ptr+len) = '\0';
            break;
        }

        /* disconnected */
        if (n == 0) {
            exits("DISCONNECTED");
        }

        len = len + 1;
    }

    return len;
}

/* count tabs in a line */

int counttabs(char *lin) {
    int i, n = 0;

    for (i = 0; i < strlen(lin); i++) {
        if (lin[i] == '\t') {
            n++;
        }
    }

    return n;
}
