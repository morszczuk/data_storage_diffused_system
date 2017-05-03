# data_storage_diffused_system

### Description
Project created for UNIX course.
Allows user to upload file and then stores it in various slave nodes. File can be downloaded through HTTP client (f.e. via browser).
File should be available for downloading even after certain number of nodes will be quitted.

### Example usage
To see example usage, in main directory run command:

```
./example/launch_script_valgrind.sh
```

It will run:
- main server task on port 9000
- three slave nodes on ports 4000, 5000 and 6000
- administration client thath will show all of the current operations on server
- upload client responsible for uploading file example/example_file to the system

### Details
Deatiled description of task is available:
[project Djk, http://www.mini.pw.edu.pl/~marcinbo/strona/zadania/unixprojekt2016.html]
