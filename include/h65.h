// Job completion codes for H65 page fetching

// Could not connect to web server
#define H65_COULDNOTCONNECT 1
// Could not send HTTP request
#define H65_SENDHTTP 2
// Did not find H65 marker in the page
#define H65_BEFORE 3
// Unsupported H65 content version
#define H65_VERSIONMISMATCH 4
// Invalid block definition in H65 stream
#define H65_BADBLOCK 5
// socket_connect() failed
#define H65_CONNECTFAILED 6
// Everything finished just fine
#define H65_DONE 255
