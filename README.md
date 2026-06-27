# httpserver
It serves the file home.txt(at path "/") and file.txt(at path "local/file.txt")
for client use 
echo "GET /file.txt HTTP/1.1"|nc localhost 3490
or echo "GET / HTTP/1.1"|nc localhost 3490

Ctrl+C to close server