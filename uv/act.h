#ifndef ACT_H
#define ACT_H 0
typedef struct act_request{
    char *_interface;                   int interface_len;
    char *method;                       int method_len;
    char *parameter_types_string;       int pts_len;
    int id;
    int p_len;
    char *parameter;
}act_request;

typedef struct act_response{
    int id;
    int data_len;
    char *result;
}act_response;

#endif
