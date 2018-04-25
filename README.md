gophers
-------
Sean Caron scaron@umich.edu

### Installation

1. acme gophers.c

2. specify the FQDN of the host on which the server is running in the HOST_NAME #define.

3. mk

4. cp ./gophers /usr/you/bin/yourarch

### Usage

./gophers [port] [gopher root directory] [logfile]

### Notes

all command line parameters are mandatory.

the gopher server serves up documents from the gopher root directory. the theory
of operation is generally as follows:

```
repeat forever {
	get a request from the client

	catenate the request path with the root directory to get a real filesystem path

	stat this file to see if it is a file or directory.

	if (file) {
		spit file out to client
	}

	if (directory) {
		look for .gopher file

		if (exists) {

			assume it is in 'gopher markup language' and spit it directly to the client

		}

		else {

			generate a basic directory listing in 'gopher markup language' and send it
			to the client

		}
	}
}
```

### Markup language and .gopher files

what i am calling gopher markup language is really just lines of text formatted in a way that
is rfc 1436 (gopher rfc) compliant. that is, lines of text that the client will interpret as valid
gopher selectors. they must look like this

```
[type][selector]\t[path]\t[server name]\t[port]\n
```

(where \t=tab and \n=hit RETURN)

[type] is a one-character code that describes the filetype, it should be one of the following

0=plaintext
1=directory
2=cso search query
3=error message
4=binhex archive
5=binary archive
6=uuencoded
7=search engine query
8=telnet link
9=binary file
d=pdf file
g=gif image file
h=html file
i=informational file (just displayed as text)
l=image file (e.g. jpg)
s=audio file

some of these seem to be kind of de facto additions to the original standard and might not be
supported by all gopher browsers.

[selector] is the text of the link that is visible to the user

[path] is relative to the gopher root, make sure you begin it with a leading slash.

[server name] is the fqdn of the gopher server

[port] is the port number on which the gopher server is running.

if you just type a 'plain text line' into a .gopher file the server will automatically make an i-line out of it.
by 'plain text line' i mean a line that does not contain three tabs and has a first character equal to one of
the filetype codes above.

note that i don't support the h-line URL:http://blah redirect, just file type information only.

if you are hand-coding an i-line it is customary to have a null path, host as error.host and port 1.

after typing the last line in your .gopher file, hit return, so that the file ends with a blank new line.
