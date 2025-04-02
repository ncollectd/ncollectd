// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <sys/stat.h>
#include <sys/un.h>
#include <curl/curl.h>

#include "plugins/haproxy/haproxy_process_fams.h"
#include "plugins/haproxy/haproxy_stat_fams.h"
#include "plugins/haproxy/haproxy_info.h"
#include "plugins/haproxy/haproxy_stat.h"

enum {
    FAM_HAPROXY_STICKTABLE_SIZE,
    FAM_HAPROXY_STICKTABLE_USED,
    FAM_HAPROXY_STICKTABLE_MAX,
};

typedef enum {
    HA_TYPE_FRONTEND,
    HA_TYPE_BACKEND,
    HA_TYPE_SERVER,
    HA_TYPE_LISTENER,
} ha_type_t;

typedef enum {
    HA_PROXY_MODE_TCP,
    HA_PROXY_MODE_HTTP,
    HA_PROXY_MODE_HEALTH,
    HA_PROXY_MODE_UNKNOW,
} ha_proxy_mode_t ;

typedef struct {
    char *instance;
    label_set_t labels;
    plugin_filter_t *filter;

    char *socketpath;

    char *url;
    int address_family;
    char *user;
    char *pass;
    char *credentials;
    bool digest;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    struct curl_slist *headers;

    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    char *buffer;
    size_t buffer_size;
    size_t buffer_fill;

    metric_family_t fams_process[FAM_HAPROXY_PROCESS_MAX];
    metric_family_t fams_stat[FAM_HAPROXY_STAT_MAX];
    metric_family_t fams_sticktable[FAM_HAPROXY_STICKTABLE_MAX];
} haproxy_t;

typedef struct {
    int field;
    int fam;
} haproxy_field_t;

static haproxy_field_t haproxy_frontend_fields[] = {
    { HA_STAT_SCUR,                                            FAM_HAPROXY_FRONTEND_CURRENT_SESSIONS                          },
    { HA_STAT_SMAX,                                            FAM_HAPROXY_FRONTEND_MAX_SESSIONS                              },
    { HA_STAT_SLIM,                                            FAM_HAPROXY_FRONTEND_LIMIT_SESSION                             },
    { HA_STAT_STOT,                                            FAM_HAPROXY_FRONTEND_SESSIONS                                  },
    { HA_STAT_BIN,                                             FAM_HAPROXY_FRONTEND_BYTES_IN                                  },
    { HA_STAT_BOUT,                                            FAM_HAPROXY_FRONTEND_BYTES_OUT                                 },
    { HA_STAT_DREQ,                                            FAM_HAPROXY_FRONTEND_REQUESTS_DENIED                           },
    { HA_STAT_DRESP,                                           FAM_HAPROXY_FRONTEND_RESPONSES_DENIED                          },
    { HA_STAT_EREQ,                                            FAM_HAPROXY_FRONTEND_REQUEST_ERRORS                            },
    { HA_STAT_STATUS,                                          FAM_HAPROXY_FRONTEND_STATUS                                    },
    { HA_STAT_RATE_LIM,                                        FAM_HAPROXY_FRONTEND_LIMIT_SESSION_RATE                        },
    { HA_STAT_RATE_MAX,                                        FAM_HAPROXY_FRONTEND_MAX_SESSION_RATE                          },
    { HA_STAT_HRSP_1XX,                                        FAM_HAPROXY_FRONTEND_HTTP_RESPONSES                            },
    { HA_STAT_HRSP_2XX,                                        FAM_HAPROXY_FRONTEND_HTTP_RESPONSES                            },
    { HA_STAT_HRSP_3XX,                                        FAM_HAPROXY_FRONTEND_HTTP_RESPONSES                            },
    { HA_STAT_HRSP_4XX,                                        FAM_HAPROXY_FRONTEND_HTTP_RESPONSES                            },
    { HA_STAT_HRSP_5XX,                                        FAM_HAPROXY_FRONTEND_HTTP_RESPONSES                            },
    { HA_STAT_HRSP_OTHER,                                      FAM_HAPROXY_FRONTEND_HTTP_RESPONSES                            },
    { HA_STAT_REQ_RATE_MAX,                                    FAM_HAPROXY_FRONTEND_HTTP_REQUESTS_RATE_MAX                    },
    { HA_STAT_REQ_TOT,                                         FAM_HAPROXY_FRONTEND_HTTP_REQUESTS                             },
    { HA_STAT_COMP_IN,                                         FAM_HAPROXY_FRONTEND_HTTP_COMP_BYTES_IN                        },
    { HA_STAT_COMP_OUT,                                        FAM_HAPROXY_FRONTEND_HTTP_COMP_BYTES_OUT                       },
    { HA_STAT_COMP_BYP,                                        FAM_HAPROXY_FRONTEND_HTTP_COMP_BYTES_BYPASSED                  },
    { HA_STAT_COMP_RSP,                                        FAM_HAPROXY_FRONTEND_HTTP_COMP_RESPONSES                       },
    { HA_STAT_CONN_RATE_MAX,                                   FAM_HAPROXY_FRONTEND_CONNECTIONS_RATE_MAX                      },
    { HA_STAT_CONN_TOT,                                        FAM_HAPROXY_FRONTEND_CONNECTIONS                               },
    { HA_STAT_INTERCEPTED,                                     FAM_HAPROXY_FRONTEND_INTERCEPTED_REQUESTS                      },
    { HA_STAT_DCON,                                            FAM_HAPROXY_FRONTEND_DENIED_CONNECTIONS                        },
    { HA_STAT_DSES,                                            FAM_HAPROXY_FRONTEND_DENIED_SESSIONS                           },
    { HA_STAT_WREW,                                            FAM_HAPROXY_FRONTEND_FAILED_HEADER_REWRITING                   },
    { HA_STAT_CACHE_LOOKUPS,                                   FAM_HAPROXY_FRONTEND_HTTP_CACHE_LOOKUPS                        },
    { HA_STAT_CACHE_HITS,                                      FAM_HAPROXY_FRONTEND_HTTP_CACHE_HITS                           },
    { HA_STAT_EINT,                                            FAM_HAPROXY_FRONTEND_INTERNAL_ERRORS                           },
    { HA_STAT_SESS_OTHER,                                      FAM_HAPROXY_FRONTEND_OTHER_SESSIONS                            },
    { HA_STAT_H1SESS,                                          FAM_HAPROXY_FRONTEND_H1_SESSIONS                               },
    { HA_STAT_H2SESS,                                          FAM_HAPROXY_FRONTEND_H2_SESSIONS                               },
    { HA_STAT_H3SESS,                                          FAM_HAPROXY_FRONTEND_H3_SESSIONS                               },
    { HA_STAT_REQ_OTHER,                                       FAM_HAPROXY_FRONTEND_OTHER_REQUEST                             },
    { HA_STAT_H1REQ,                                           FAM_HAPROXY_FRONTEND_H1_REQUEST                                },
    { HA_STAT_H2REQ,                                           FAM_HAPROXY_FRONTEND_H2_REQUEST                                },
    { HA_STAT_H3REQ,                                           FAM_HAPROXY_FRONTEND_H3_REQUEST                                },
    { HA_STAT_EXTRA_SSL_SESS,                                  FAM_HAPROXY_FRONTEND_SSL_SESSIONS                              },
    { HA_STAT_EXTRA_SSL_REUSED_SESS,                           FAM_HAPROXY_FRONTEND_SSL_REUSE_SESSIONS                        },
    { HA_STAT_EXTRA_SSL_FAILED_HANDSHAKE,                      FAM_HAPROXY_FRONTEND_SSL_FAILED_HANDSHAKE                      },
    { HA_STAT_EXTRA_SSL_OCSP_STAPLE,                           FAM_HAPROXY_FRONTEND_SSL_OCSP_STAPLE                           },
    { HA_STAT_EXTRA_SSL_FAILED_OCSP_STAPLE,                    FAM_HAPROXY_FRONTEND_SSL_FAILED_OCSP_STAPLE                    },
    { HA_STAT_EXTRA_H2_HEADERS_RCVD,                           FAM_HAPROXY_FRONTEND_H2_HEADERS_RCVD                           },
    { HA_STAT_EXTRA_H2_DATA_RCVD,                              FAM_HAPROXY_FRONTEND_H2_DATA_RCVD                              },
    { HA_STAT_EXTRA_H2_SETTINGS_RCVD,                          FAM_HAPROXY_FRONTEND_H2_SETTINGS_RCVD                          },
    { HA_STAT_EXTRA_H2_RST_STREAM_RCVD,                        FAM_HAPROXY_FRONTEND_H2_RST_STREAM_RCVD                        },
    { HA_STAT_EXTRA_H2_GOAWAY_RCVD,                            FAM_HAPROXY_FRONTEND_H2_GOAWAY_RCVD                            },
    { HA_STAT_EXTRA_H2_DETECTED_CONN_PROTOCOL_ERRORS,          FAM_HAPROXY_FRONTEND_H2_DETECTED_CONNECTION_PROTOCOL_ERRORS    },
    { HA_STAT_EXTRA_H2_DETECTED_STRM_PROTOCOL_ERRORS,          FAM_HAPROXY_FRONTEND_H2_DETECTED_STREAM_PROTOCOL_ERRORS        },
    { HA_STAT_EXTRA_H2_RST_STREAM_RESP,                        FAM_HAPROXY_FRONTEND_H2_RST_STREAM_RESP                        },
    { HA_STAT_EXTRA_H2_GOAWAY_RESP,                            FAM_HAPROXY_FRONTEND_H2_GOAWAY_RESP                            },
    { HA_STAT_EXTRA_H2_OPEN_CONNECTIONS,                       FAM_HAPROXY_FRONTEND_H2_OPEN_CONNECTIONS                       },
    { HA_STAT_EXTRA_H2_BACKEND_OPEN_STREAMS,                   FAM_HAPROXY_FRONTEND_H2_BACKEND_OPEN_STREAMS                   },
    { HA_STAT_EXTRA_H2_TOTAL_CONNECTIONS,                      FAM_HAPROXY_FRONTEND_H2_CONNECTIONS                            },
    { HA_STAT_EXTRA_H2_BACKEND_TOTAL_STREAMS,                  FAM_HAPROXY_FRONTEND_H2_BACKEND_STREAMS                        },
    { HA_STAT_EXTRA_H1_OPEN_CONNECTIONS,                       FAM_HAPROXY_FRONTEND_H1_OPEN_CONNECTIONS                       },
    { HA_STAT_EXTRA_H1_OPEN_STREAMS,                           FAM_HAPROXY_FRONTEND_H1_OPEN_STREAMS                           },
    { HA_STAT_EXTRA_H1_TOTAL_CONNECTIONS,                      FAM_HAPROXY_FRONTEND_H1_CONNECTIONS                            },
    { HA_STAT_EXTRA_H1_TOTAL_STREAMS,                          FAM_HAPROXY_FRONTEND_H1_STREAMS                                },
    { HA_STAT_EXTRA_H1_BYTES_IN,                               FAM_HAPROXY_FRONTEND_H1_IN_BYTES                               },
    { HA_STAT_EXTRA_H1_BYTES_OUT,                              FAM_HAPROXY_FRONTEND_H1_OUT_BYTES                              },
    { HA_STAT_EXTRA_H1_SPLICED_BYTES_IN,                       FAM_HAPROXY_FRONTEND_H1_SPLICED_IN_BYTES                       },
    { HA_STAT_EXTRA_H1_SPLICED_BYTES_OUT,                      FAM_HAPROXY_FRONTEND_H1_SPLICED_OUT_BYTES                      },
    { HA_STAT_EXTRA_H3_DATA,                                   FAM_HAPROXY_FRONTEND_H3_DATA                                   },
    { HA_STAT_EXTRA_H3_HEADERS,                                FAM_HAPROXY_FRONTEND_H3_HEADERS                                },
    { HA_STAT_EXTRA_H3_CANCEL_PUSH,                            FAM_HAPROXY_FRONTEND_H3_CANCEL_PUSH                            },
    { HA_STAT_EXTRA_H3_PUSH_PROMISE,                           FAM_HAPROXY_FRONTEND_H3_PUSH_PROMISE                           },
    { HA_STAT_EXTRA_H3_MAX_PUSH_ID,                            FAM_HAPROXY_FRONTEND_H3_MAX_PUSH_ID                            },
    { HA_STAT_EXTRA_H3_GOAWAY,                                 FAM_HAPROXY_FRONTEND_H3_GOAWAY                                 },
    { HA_STAT_EXTRA_H3_SETTINGS,                               FAM_HAPROXY_FRONTEND_H3_SETTINGS                               },
    { HA_STAT_EXTRA_H3_NO_ERROR,                               FAM_HAPROXY_FRONTEND_H3_NO_ERROR                               },
    { HA_STAT_EXTRA_H3_GENERAL_PROTOCOL_ERROR,                 FAM_HAPROXY_FRONTEND_H3_GENERAL_PROTOCOL_ERROR                 },
    { HA_STAT_EXTRA_H3_INTERNAL_ERROR,                         FAM_HAPROXY_FRONTEND_H3_INTERNAL_ERROR                         },
    { HA_STAT_EXTRA_H3_STREAM_CREATION_ERROR,                  FAM_HAPROXY_FRONTEND_H3_STREAM_CREATION_ERROR                  },
    { HA_STAT_EXTRA_H3_CLOSED_CRITICAL_STREAM,                 FAM_HAPROXY_FRONTEND_H3_CLOSED_CRITICAL_STREAM                 },
    { HA_STAT_EXTRA_H3_FRAME_UNEXPECTED,                       FAM_HAPROXY_FRONTEND_H3_FRAME_UNEXPECTED                       },
    { HA_STAT_EXTRA_H3_FRAME_ERROR,                            FAM_HAPROXY_FRONTEND_H3_FRAME_ERROR                            },
    { HA_STAT_EXTRA_H3_EXCESSIVE_LOAD,                         FAM_HAPROXY_FRONTEND_H3_EXCESSIVE_LOAD                         },
    { HA_STAT_EXTRA_H3_ID_ERROR,                               FAM_HAPROXY_FRONTEND_H3_ID_ERROR                               },
    { HA_STAT_EXTRA_H3_SETTINGS_ERROR,                         FAM_HAPROXY_FRONTEND_H3_SETTINGS_ERROR                         },
    { HA_STAT_EXTRA_H3_MISSING_SETTINGS,                       FAM_HAPROXY_FRONTEND_H3_MISSING_SETTINGS                       },
    { HA_STAT_EXTRA_H3_REQUEST_REJECTED,                       FAM_HAPROXY_FRONTEND_H3_REQUEST_REJECTED                       },
    { HA_STAT_EXTRA_H3_REQUEST_CANCELLED,                      FAM_HAPROXY_FRONTEND_H3_REQUEST_CANCELLED                      },
    { HA_STAT_EXTRA_H3_REQUEST_INCOMPLETE,                     FAM_HAPROXY_FRONTEND_H3_REQUEST_INCOMPLETE                     },
    { HA_STAT_EXTRA_H3_MESSAGE_ERROR,                          FAM_HAPROXY_FRONTEND_H3_MESSAGE_ERROR                          },
    { HA_STAT_EXTRA_H3_CONNECT_ERROR,                          FAM_HAPROXY_FRONTEND_H3_CONNECT_ERROR                          },
    { HA_STAT_EXTRA_H3_VERSION_FALLBACK,                       FAM_HAPROXY_FRONTEND_H3_VERSION_FALLBACK                       },
    { HA_STAT_EXTRA_H3_QPACK_DECOMPRESSION_FAILED,             FAM_HAPROXY_FRONTEND_H3_QPACK_DECOMPRESSION_FAILED             },
    { HA_STAT_EXTRA_H3_QPACK_ENCODER_STREAM_ERROR,             FAM_HAPROXY_FRONTEND_H3_QPACK_ENCODER_STREAM_ERROR             },
    { HA_STAT_EXTRA_H3_QPACK_DECODER_STREAM_ERROR,             FAM_HAPROXY_FRONTEND_H3_QPACK_DECODER_STREAM_ERROR             },
    { HA_STAT_EXTRA_QUIC_DROPPED_PACKET,                       FAM_HAPROXY_FRONTEND_QUIC_DROPPED_PACKET                       },
    { HA_STAT_EXTRA_QUIC_DROPPED_PARSING,                      FAM_HAPROXY_FRONTEND_QUIC_DROPPED_PARSING                      },
    { HA_STAT_EXTRA_QUIC_LOST_PACKET,                          FAM_HAPROXY_FRONTEND_QUIC_LOST_PACKET                          },
    { HA_STAT_EXTRA_QUIC_TOO_SHORT_INITIAL_DGRAM,              FAM_HAPROXY_FRONTEND_QUIC_TOO_SHORT_INITIAL_DGRAM              },
    { HA_STAT_EXTRA_QUIC_RETRY_SENT,                           FAM_HAPROXY_FRONTEND_QUIC_RETRY_SENT                           },
    { HA_STAT_EXTRA_QUIC_RETRY_VALIDATED,                      FAM_HAPROXY_FRONTEND_QUIC_RETRY_VALIDATED                      },
    { HA_STAT_EXTRA_QUIC_RETRY_ERRORS,                         FAM_HAPROXY_FRONTEND_QUIC_RETRY_ERRORS                         },
    { HA_STAT_EXTRA_QUIC_HALF_OPEN_CONN,                       FAM_HAPROXY_FRONTEND_QUIC_HALF_OPEN_CONN                       },
    { HA_STAT_EXTRA_QUIC_HDSHK_FAIL,                           FAM_HAPROXY_FRONTEND_QUIC_HDSHK_FAIL                           },
    { HA_STAT_EXTRA_QUIC_STATELESS_RESET_SENT,                 FAM_HAPROXY_FRONTEND_QUIC_STATELESS_RESET_SENT                 },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_NO_ERROR,                  FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_NO_ERROR                  },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_INTERNAL_ERROR,            FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_INTERNAL_ERROR            },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_CONNECTION_REFUSED,        FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_CONNECTION_REFUSED        },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_FLOW_CONTROL_ERROR,        FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_FLOW_CONTROL_ERROR        },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_STREAM_LIMIT_ERROR,        FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_STREAM_LIMIT_ERROR        },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_STREAM_STATE_ERROR,        FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_STREAM_STATE_ERROR        },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_FINAL_SIZE_ERROR,          FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_FINAL_SIZE_ERROR          },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_FRAME_ENCODING_ERROR,      FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_FRAME_ENCODING_ERROR      },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_TRANSPORT_PARAMETER_ERROR, FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_TRANSPORT_PARAMETER_ERROR },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_CONNECTION_ID_LIMIT_ERROR, FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_CONNECTION_ID_LIMIT_ERROR },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_PROTOCOL_VIOLATION,        FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_PROTOCOL_VIOLATION        },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_INVALID_TOKEN,             FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_INVALID_TOKEN             },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_APPLICATION_ERROR,         FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_APPLICATION_ERROR         },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_CRYPTO_BUFFER_EXCEEDED,    FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_CRYPTO_BUFFER_EXCEEDED    },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_KEY_UPDATE_ERROR,          FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_KEY_UPDATE_ERROR          },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_AEAD_LIMIT_REACHED,        FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_AEAD_LIMIT_REACHED        },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_NO_VIABLE_PATH,            FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_NO_VIABLE_PATH            },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_CRYPTO_ERROR,              FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_CRYPTO_ERROR              },
    { HA_STAT_EXTRA_QUIC_TRANSP_ERR_UNKNOWN_ERROR,             FAM_HAPROXY_FRONTEND_QUIC_TRANSP_ERR_UNKNOWN_ERROR             },
    { HA_STAT_EXTRA_QUIC_DATA_BLOCKED,                         FAM_HAPROXY_FRONTEND_QUIC_DATA_BLOCKED                         },
    { HA_STAT_EXTRA_QUIC_STREAM_DATA_BLOCKED,                  FAM_HAPROXY_FRONTEND_QUIC_STREAM_DATA_BLOCKED                  },
    { HA_STAT_EXTRA_QUIC_STREAMS_BLOCKED_BIDI,                 FAM_HAPROXY_FRONTEND_QUIC_STREAMS_BLOCKED_BIDI                 },
    { HA_STAT_EXTRA_QUIC_STREAMS_BLOCKED_UNI,                  FAM_HAPROXY_FRONTEND_QUIC_STREAMS_BLOCKED_UNI                  },
};
static size_t haproxy_frontend_fields_size = STATIC_ARRAY_SIZE(haproxy_frontend_fields);

static haproxy_field_t haproxy_listener_fields[] = {
    { HA_STAT_SCUR,                        FAM_HAPROXY_LISTENER_CURRENT_SESSIONS        },
    { HA_STAT_SMAX,                        FAM_HAPROXY_LISTENER_MAX_SESSIONS            },
    { HA_STAT_SLIM,                        FAM_HAPROXY_LISTENER_LIMIT_SESSIONS          },
    { HA_STAT_STOT,                        FAM_HAPROXY_LISTENER_SESSIONS                },
    { HA_STAT_BIN,                         FAM_HAPROXY_LISTENER_BYTES_IN                },
    { HA_STAT_BOUT,                        FAM_HAPROXY_LISTENER_BYTES_OUT               },
    { HA_STAT_DREQ,                        FAM_HAPROXY_LISTENER_REQUESTS_DENIED         },
    { HA_STAT_DRESP,                       FAM_HAPROXY_LISTENER_RESPONSES_DENIED        },
    { HA_STAT_EREQ,                        FAM_HAPROXY_LISTENER_REQUEST_ERRORS          },
    { HA_STAT_STATUS,                      FAM_HAPROXY_LISTENER_STATUS                  },
    { HA_STAT_DCON,                        FAM_HAPROXY_LISTENER_DENIED_CONNECTIONS      },
    { HA_STAT_DSES,                        FAM_HAPROXY_LISTENER_DENIED_SESSIONS         },
    { HA_STAT_WREW,                        FAM_HAPROXY_LISTENER_FAILED_HEADER_REWRITING },
    { HA_STAT_EINT,                        FAM_HAPROXY_LISTENER_INTERNAL_ERRORS         },
    { HA_STAT_EXTRA_SSL_SESS,              FAM_HAPROXY_LISTENER_SSL_SESSIONS            },
    { HA_STAT_EXTRA_SSL_REUSED_SESS,       FAM_HAPROXY_LISTENER_SSL_REUSE_SESSIONS      },
    { HA_STAT_EXTRA_SSL_FAILED_HANDSHAKE,  FAM_HAPROXY_LISTENER_SSL_FAILED_HANDSHAKE    }
};
static size_t haproxy_listener_fields_size = STATIC_ARRAY_SIZE(haproxy_listener_fields);

static haproxy_field_t haproxy_backend_fields[] = {
    { HA_STAT_QCUR,                                   FAM_HAPROXY_BACKEND_CURRENT_QUEUE                          },
    { HA_STAT_QMAX,                                   FAM_HAPROXY_BACKEND_MAX_QUEUE                              },
    { HA_STAT_SCUR,                                   FAM_HAPROXY_BACKEND_CURRENT_SESSIONS                       },
    { HA_STAT_SMAX,                                   FAM_HAPROXY_BACKEND_MAX_SESSIONS                           },
    { HA_STAT_SLIM,                                   FAM_HAPROXY_BACKEND_LIMIT_SESSIONS                         },
    { HA_STAT_STOT,                                   FAM_HAPROXY_BACKEND_SESSIONS                               },
    { HA_STAT_BIN,                                    FAM_HAPROXY_BACKEND_BYTES_IN                               },
    { HA_STAT_BOUT,                                   FAM_HAPROXY_BACKEND_BYTES_OUT                              },
    { HA_STAT_DREQ,                                   FAM_HAPROXY_BACKEND_REQUESTS_DENIED                        },
    { HA_STAT_DRESP,                                  FAM_HAPROXY_BACKEND_RESPONSES_DENIED                       },
    { HA_STAT_ECON,                                   FAM_HAPROXY_BACKEND_CONNECTION_ERRORS                      },
    { HA_STAT_ERESP,                                  FAM_HAPROXY_BACKEND_RESPONSE_ERRORS                        },
    { HA_STAT_WRETR,                                  FAM_HAPROXY_BACKEND_RETRY_WARNINGS                         },
    { HA_STAT_WREDIS,                                 FAM_HAPROXY_BACKEND_REDISPATCH_WARNINGS                    },
    { HA_STAT_STATUS,                                 FAM_HAPROXY_BACKEND_STATUS                                 },
    { HA_STAT_WEIGHT,                                 FAM_HAPROXY_BACKEND_WEIGHT                                 },
    { HA_STAT_ACT,                                    FAM_HAPROXY_BACKEND_ACTIVE_SERVERS                         },
    { HA_STAT_BCK,                                    FAM_HAPROXY_BACKEND_BACKUP_SERVERS                         },
    { HA_STAT_CHKFAIL,                                FAM_HAPROXY_BACKEND_CHECK_FAILURES                         },
    { HA_STAT_CHKDOWN,                                FAM_HAPROXY_BACKEND_CHECK_UP_DOWN                          },
    { HA_STAT_LASTCHG,                                FAM_HAPROXY_BACKEND_CHECK_LAST_CHANGE_SECONDS              },
    { HA_STAT_DOWNTIME,                               FAM_HAPROXY_BACKEND_DOWNTIME_SECONDS                       },
    { HA_STAT_LBTOT,                                  FAM_HAPROXY_BACKEND_LOADBALANCED                           },
    { HA_STAT_RATE_MAX,                               FAM_HAPROXY_BACKEND_MAX_SESSION_RATE                       },
    { HA_STAT_HRSP_1XX,                               FAM_HAPROXY_BACKEND_HTTP_RESPONSES                         },
    { HA_STAT_HRSP_2XX,                               FAM_HAPROXY_BACKEND_HTTP_RESPONSES                         },
    { HA_STAT_HRSP_3XX,                               FAM_HAPROXY_BACKEND_HTTP_RESPONSES                         },
    { HA_STAT_HRSP_4XX,                               FAM_HAPROXY_BACKEND_HTTP_RESPONSES                         },
    { HA_STAT_HRSP_5XX,                               FAM_HAPROXY_BACKEND_HTTP_RESPONSES                         },
    { HA_STAT_HRSP_OTHER,                             FAM_HAPROXY_BACKEND_HTTP_RESPONSES                         },
    { HA_STAT_REQ_TOT,                                FAM_HAPROXY_BACKEND_HTTP_REQUESTS                          },
    { HA_STAT_CLI_ABRT,                               FAM_HAPROXY_BACKEND_CLIENT_ABORTS                          },
    { HA_STAT_SRV_ABRT,                               FAM_HAPROXY_BACKEND_SERVER_ABORTS                          },
    { HA_STAT_COMP_IN,                                FAM_HAPROXY_BACKEND_HTTP_COMP_BYTES_IN                     },
    { HA_STAT_COMP_OUT,                               FAM_HAPROXY_BACKEND_HTTP_COMP_BYTES_OUT                    },
    { HA_STAT_COMP_BYP,                               FAM_HAPROXY_BACKEND_HTTP_COMP_BYTES_BYPASSED               },
    { HA_STAT_COMP_RSP,                               FAM_HAPROXY_BACKEND_HTTP_COMP_RESPONSES                    },
    { HA_STAT_LASTSESS,                               FAM_HAPROXY_BACKEND_LAST_SESSION_SECONDS                   },
    { HA_STAT_QTIME,                                  FAM_HAPROXY_BACKEND_QUEUE_TIME_AVERAGE_SECONDS             },
    { HA_STAT_CTIME,                                  FAM_HAPROXY_BACKEND_CONNECT_TIME_AVERAGE_SECONDS           },
    { HA_STAT_RTIME,                                  FAM_HAPROXY_BACKEND_RESPONSE_TIME_AVERAGE_SECONDS          },
    { HA_STAT_TTIME,                                  FAM_HAPROXY_BACKEND_TOTAL_TIME_AVERAGE_SECONDS             },
    { HA_STAT_WREW,                                   FAM_HAPROXY_BACKEND_FAILED_HEADER_REWRITING                },
    { HA_STAT_CONNECT,                                FAM_HAPROXY_BACKEND_CONNECTION_ATTEMPTS                    },
    { HA_STAT_REUSE,                                  FAM_HAPROXY_BACKEND_CONNECTION_REUSES                      },
    { HA_STAT_CACHE_LOOKUPS,                          FAM_HAPROXY_BACKEND_HTTP_CACHE_LOOKUPS                     },
    { HA_STAT_CACHE_HITS,                             FAM_HAPROXY_BACKEND_HTTP_CACHE_HITS                        },
    { HA_STAT_QT_MAX,                                 FAM_HAPROXY_BACKEND_MAX_QUEUE_TIME_SECONDS                 },
    { HA_STAT_CT_MAX,                                 FAM_HAPROXY_BACKEND_MAX_CONNECT_TIME_SECONDS               },
    { HA_STAT_RT_MAX,                                 FAM_HAPROXY_BACKEND_MAX_RESPONSE_TIME_SECONDS              },
    { HA_STAT_TT_MAX,                                 FAM_HAPROXY_BACKEND_MAX_TOTAL_TIME_SECONDS                 },
    { HA_STAT_EINT,                                   FAM_HAPROXY_BACKEND_INTERNAL_ERRORS                        },
    { HA_STAT_UWEIGHT,                                FAM_HAPROXY_BACKEND_UWEIGHT                                },
    { HA_STAT_EXTRA_SSL_SESS,                         FAM_HAPROXY_BACKEND_SSL_SESSIONS                           },
    { HA_STAT_EXTRA_SSL_REUSED_SESS,                  FAM_HAPROXY_BACKEND_SSL_REUSE_SESSIONS                     },
    { HA_STAT_EXTRA_SSL_FAILED_HANDSHAKE,             FAM_HAPROXY_BACKEND_SSL_FAILED_HANDSHAKE                   },
    { HA_STAT_EXTRA_H2_HEADERS_RCVD,                  FAM_HAPROXY_BACKEND_H2_HEADERS_RCVD                        },
    { HA_STAT_EXTRA_H2_DATA_RCVD,                     FAM_HAPROXY_BACKEND_H2_DATA_RCVD                           },
    { HA_STAT_EXTRA_H2_SETTINGS_RCVD,                 FAM_HAPROXY_BACKEND_H2_SETTINGS_RCVD                       },
    { HA_STAT_EXTRA_H2_RST_STREAM_RCVD,               FAM_HAPROXY_BACKEND_H2_RST_STREAM_RCVD                     },
    { HA_STAT_EXTRA_H2_GOAWAY_RCVD,                   FAM_HAPROXY_BACKEND_H2_GOAWAY_RCVD                         },
    { HA_STAT_EXTRA_H2_DETECTED_CONN_PROTOCOL_ERRORS, FAM_HAPROXY_BACKEND_H2_DETECTED_CONNECTION_PROTOCOL_ERRORS },
    { HA_STAT_EXTRA_H2_DETECTED_STRM_PROTOCOL_ERRORS, FAM_HAPROXY_BACKEND_H2_DETECTED_STREAM_PROTOCOL_ERRORS     },
    { HA_STAT_EXTRA_H2_RST_STREAM_RESP,               FAM_HAPROXY_BACKEND_H2_RST_STREAM_RESP                     },
    { HA_STAT_EXTRA_H2_GOAWAY_RESP,                   FAM_HAPROXY_BACKEND_H2_GOAWAY_RESP                         },
    { HA_STAT_EXTRA_H2_OPEN_CONNECTIONS,              FAM_HAPROXY_BACKEND_H2_OPEN_CONNECTIONS                    },
    { HA_STAT_EXTRA_H2_BACKEND_OPEN_STREAMS,          FAM_HAPROXY_BACKEND_H2_BACKEND_OPEN_STREAMS                },
    { HA_STAT_EXTRA_H2_TOTAL_CONNECTIONS,             FAM_HAPROXY_BACKEND_H2_CONNECTIONS                         },
    { HA_STAT_EXTRA_H2_BACKEND_TOTAL_STREAMS,         FAM_HAPROXY_BACKEND_H2_BACKEND_STREAMS                     },
    { HA_STAT_EXTRA_H1_OPEN_CONNECTIONS,              FAM_HAPROXY_BACKEND_H1_OPEN_CONNECTIONS                    },
    { HA_STAT_EXTRA_H1_OPEN_STREAMS,                  FAM_HAPROXY_BACKEND_H1_OPEN_STREAMS                        },
    { HA_STAT_EXTRA_H1_TOTAL_CONNECTIONS,             FAM_HAPROXY_BACKEND_H1_CONNECTIONS                         },
    { HA_STAT_EXTRA_H1_TOTAL_STREAMS,                 FAM_HAPROXY_BACKEND_H1_STREAMS                             },
    { HA_STAT_EXTRA_H1_BYTES_IN,                      FAM_HAPROXY_BACKEND_H1_IN_BYTES                            },
    { HA_STAT_EXTRA_H1_BYTES_OUT,                     FAM_HAPROXY_BACKEND_H1_OUT_BYTES                           },
    { HA_STAT_EXTRA_H1_SPLICED_BYTES_IN,              FAM_HAPROXY_BACKEND_H1_SPLICED_IN_BYTES                    },
    { HA_STAT_EXTRA_H1_SPLICED_BYTES_OUT,             FAM_HAPROXY_BACKEND_H1_SPLICED_OUT_BYTES                   }
};
static size_t haproxy_backend_fields_size = STATIC_ARRAY_SIZE(haproxy_backend_fields);

static haproxy_field_t haproxy_server_fields[] = {
    { HA_STAT_QCUR,                        FAM_HAPROXY_SERVER_CURRENT_QUEUE                   },
    { HA_STAT_QMAX,                        FAM_HAPROXY_SERVER_MAX_QUEUE                       },
    { HA_STAT_SCUR,                        FAM_HAPROXY_SERVER_CURRENT_SESSIONS                },
    { HA_STAT_SMAX,                        FAM_HAPROXY_SERVER_MAX_SESSIONS                    },
    { HA_STAT_SLIM,                        FAM_HAPROXY_SERVER_LIMIT_SESSIONS                  },
    { HA_STAT_STOT,                        FAM_HAPROXY_SERVER_SESSIONS                        },
    { HA_STAT_BIN,                         FAM_HAPROXY_SERVER_BYTES_IN                        },
    { HA_STAT_BOUT,                        FAM_HAPROXY_SERVER_BYTES_OUT                       },
    { HA_STAT_DRESP,                       FAM_HAPROXY_SERVER_RESPONSES_DENIED                },
    { HA_STAT_ECON,                        FAM_HAPROXY_SERVER_CONNECTION_ERRORS               },
    { HA_STAT_ERESP,                       FAM_HAPROXY_SERVER_RESPONSE_ERRORS                 },
    { HA_STAT_WRETR,                       FAM_HAPROXY_SERVER_RETRY_WARNINGS                  },
    { HA_STAT_WREDIS,                      FAM_HAPROXY_SERVER_REDISPATCH_WARNINGS             },
    { HA_STAT_STATUS,                      FAM_HAPROXY_SERVER_STATUS                          },
    { HA_STAT_WEIGHT,                      FAM_HAPROXY_SERVER_WEIGHT                          },
    { HA_STAT_CHKFAIL,                     FAM_HAPROXY_SERVER_CHECK_FAILURES                  },
    { HA_STAT_CHKDOWN,                     FAM_HAPROXY_SERVER_CHECK_UP_DOWN                   },
    { HA_STAT_LASTCHG,                     FAM_HAPROXY_SERVER_CHECK_LAST_CHANGE_SECONDS       },
    { HA_STAT_DOWNTIME,                    FAM_HAPROXY_SERVER_DOWNTIME_SECONDS                },
    { HA_STAT_QLIMIT,                      FAM_HAPROXY_SERVER_QUEUE_LIMIT                     },
    { HA_STAT_THROTTLE,                    FAM_HAPROXY_SERVER_CURRENT_THROTTLE                },
    { HA_STAT_LBTOT,                       FAM_HAPROXY_SERVER_LOADBALANCED                    },
    { HA_STAT_RATE_MAX,                    FAM_HAPROXY_SERVER_MAX_SESSION_RATE                },
    { HA_STAT_CHECK_STATUS,                FAM_HAPROXY_SERVER_CHECK_STATUS                    },
    { HA_STAT_CHECK_CODE,                  FAM_HAPROXY_SERVER_CHECK_CODE                      },
    { HA_STAT_CHECK_DURATION,              FAM_HAPROXY_SERVER_CHECK_DURATION_SECONDS          },
    { HA_STAT_HRSP_1XX,                    FAM_HAPROXY_SERVER_HTTP_RESPONSES                  },
    { HA_STAT_HRSP_2XX,                    FAM_HAPROXY_SERVER_HTTP_RESPONSES                  },
    { HA_STAT_HRSP_3XX,                    FAM_HAPROXY_SERVER_HTTP_RESPONSES                  },
    { HA_STAT_HRSP_4XX,                    FAM_HAPROXY_SERVER_HTTP_RESPONSES                  },
    { HA_STAT_HRSP_5XX,                    FAM_HAPROXY_SERVER_HTTP_RESPONSES                  },
    { HA_STAT_HRSP_OTHER,                  FAM_HAPROXY_SERVER_HTTP_RESPONSES                  },
    { HA_STAT_CLI_ABRT,                    FAM_HAPROXY_SERVER_CLIENT_ABORTS                   },
    { HA_STAT_SRV_ABRT,                    FAM_HAPROXY_SERVER_SERVER_ABORTS                   },
    { HA_STAT_LASTSESS,                    FAM_HAPROXY_SERVER_LAST_SESSION_SECONDS            },
    { HA_STAT_QTIME,                       FAM_HAPROXY_SERVER_QUEUE_TIME_AVERAGE_SECONDS      },
    { HA_STAT_CTIME,                       FAM_HAPROXY_SERVER_CONNECT_TIME_AVERAGE_SECONDS    },
    { HA_STAT_RTIME,                       FAM_HAPROXY_SERVER_RESPONSE_TIME_AVERAGE_SECONDS   },
    { HA_STAT_TTIME,                       FAM_HAPROXY_SERVER_TOTAL_TIME_AVERAGE_SECONDS      },
    { HA_STAT_WREW,                        FAM_HAPROXY_SERVER_FAILED_HEADER_REWRITING         },
    { HA_STAT_CONNECT,                     FAM_HAPROXY_SERVER_CONNECTION_ATTEMPTS             },
    { HA_STAT_REUSE,                       FAM_HAPROXY_SERVER_CONNECTION_REUSES               },
    { HA_STAT_SRV_ICUR,                    FAM_HAPROXY_SERVER_IDLE_CONNECTIONS_CURRENT        },
    { HA_STAT_SRV_ILIM,                    FAM_HAPROXY_SERVER_IDLE_CONNECTIONS_LIMIT          },
    { HA_STAT_QT_MAX,                      FAM_HAPROXY_SERVER_MAX_QUEUE_TIME_SECONDS          },
    { HA_STAT_CT_MAX,                      FAM_HAPROXY_SERVER_MAX_CONNECT_TIME_SECONDS        },
    { HA_STAT_RT_MAX,                      FAM_HAPROXY_SERVER_MAX_RESPONSE_TIME_SECONDS       },
    { HA_STAT_TT_MAX,                      FAM_HAPROXY_SERVER_MAX_TOTAL_TIME_SECONDS          },
    { HA_STAT_EINT,                        FAM_HAPROXY_SERVER_INTERNAL_ERRORS                 },
    { HA_STAT_IDLE_CONN_CUR,               FAM_HAPROXY_SERVER_UNSAFE_IDLE_CONNECTIONS_CURRENT },
    { HA_STAT_SAFE_CONN_CUR,               FAM_HAPROXY_SERVER_SAFE_IDLE_CONNECTIONS_CURRENT   },
    { HA_STAT_USED_CONN_CUR,               FAM_HAPROXY_SERVER_USED_CONNECTIONS_CURRENT        },
    { HA_STAT_NEED_CONN_EST,               FAM_HAPROXY_SERVER_NEED_CONNECTIONS_CURRENT        },
    { HA_STAT_UWEIGHT,                     FAM_HAPROXY_SERVER_UWEIGHT                         },
    { HA_STAT_EXTRA_SSL_SESS,              FAM_HAPROXY_SERVER_SSL_SESSIONS                    },
    { HA_STAT_EXTRA_SSL_REUSED_SESS,       FAM_HAPROXY_SERVER_SSL_REUSE_SESSIONS              },
    { HA_STAT_EXTRA_SSL_FAILED_HANDSHAKE,  FAM_HAPROXY_SERVER_SSL_FAILED_HANDSHAKE            }
};
static size_t haproxy_server_fields_size = STATIC_ARRAY_SIZE(haproxy_server_fields);

static metric_family_t fams_haproxy_sticktable[FAM_HAPROXY_STICKTABLE_MAX] = {
    [FAM_HAPROXY_STICKTABLE_SIZE] = {
        .name = "haproxy_sticktable_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Stick table size"
    },
    [FAM_HAPROXY_STICKTABLE_USED] = {
        .name = "haproxy_sticktable_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries used in this stick table"
    },
};

static void haproxy_free(void *arg)
{
    haproxy_t *ha = (haproxy_t *)arg;
    if (ha == NULL)
        return;

    if (ha->curl != NULL)
        curl_easy_cleanup(ha->curl);
    ha->curl = NULL;

    free(ha->instance);
    label_set_reset(&ha->labels);
    free(ha->url);
    free(ha->socketpath);
    free(ha->user);
    free(ha->pass);
    free(ha->credentials);
    free(ha->cacert);
    curl_slist_free_all(ha->headers);

    free(ha->buffer);

    free(ha);
}

static FILE *haproxy_cmd(haproxy_t *ha, char *cmd)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        PLUGIN_ERROR("socket failed: %s", STRERRNO);
        return NULL;
    }

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, ha->socketpath, sizeof(sa.sun_path)-1);
    sa.sun_path[sizeof(sa.sun_path) - 1] = 0;

    int status = connect(fd, (const struct sockaddr *) &sa, sizeof(sa));
    if (status < 0) {
        PLUGIN_ERROR("unix socket connect failed: %s", STRERRNO);
        close(fd);
        return NULL;
    }

    ssize_t cmd_len = strlen(cmd);

    if (send(fd, cmd, cmd_len, 0) < cmd_len) {
        PLUGIN_ERROR("unix socket send command failed");
        close(fd);
        return NULL;
    }

    FILE *fp = fdopen(fd, "r");
    if (fp == NULL) {
        close(fd);
        return NULL;
    }

    return fp;
}

static size_t haproxy_stat_split(char *line, char *fields[], size_t fields_size)
{
    size_t fields_len = 0;

    char *ptr = line;
    while (*ptr != '\0') {
        fields[fields_len] = ptr;
        fields_len++;
        while ((*ptr != '\0') && (*ptr != ',')) ptr++;
        if (*ptr == '\0')
            break;
        *ptr = '\0';
        ptr++;
        if (fields_len >= fields_size)
            break;
    }

     return fields_len;
}

static size_t haproxy_read_stat_header(char *line, int stat_header[HA_STAT_MAX])
{
    char *fields[512];

    size_t fields_len = haproxy_stat_split(line, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_len < (HA_STAT_MIN + 1))
        return 0;

    size_t n = 0;
    for (size_t i = 0; i < fields_len; i++) {
        const struct hastat_extra *hse = hastat_extra_get_key(fields[i], strlen(fields[i]));
        if (hse == NULL)
            continue;
        if (hse->stat < HA_STAT_MAX) {
            stat_header[hse->stat] = i;
            n++;
        }
    }

    for (int i = 0; i <= HA_STAT_MIN; i++) {
        if (stat_header[i] != i)
            return -1;
    }

    return n;
}

static int haproxy_read_stat_line(haproxy_t *ha, int stat_header[HA_STAT_MAX], char *line)
{
    char *fields[512];

    size_t fields_len = haproxy_stat_split(line, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_len < (HA_STAT_TYPE + 1))
        return -1;

    // 32. type [LFBS]: (0=frontend, 1=backend, 2=server, 3=socket/listener)
    char *type = fields[HA_STAT_TYPE];
    ha_type_t ha_type;
    switch (*type) {
    case '0':
        ha_type = HA_TYPE_FRONTEND;
        break;
    case '1':
        ha_type = HA_TYPE_BACKEND;
        break;
    case '2':
        ha_type = HA_TYPE_SERVER;
        break;
    case '3':
        ha_type = HA_TYPE_LISTENER;
        break;
    default:
        return -1;
        break;
    }

    ha_proxy_mode_t ha_proxy_mode = HA_PROXY_MODE_HTTP;
    int field_stat_mode = stat_header[HA_STAT_MODE];
    if (field_stat_mode == HA_STAT_MODE) {
        // 75: mode [LFBS]: proxy mode (tcp, http, health, unknown)
        char *mode = fields[HA_STAT_MODE];
        if (*mode != '\0') {
            if (strcmp(mode, "tcp") == 0) {
                ha_proxy_mode = HA_PROXY_MODE_TCP;
            } else if (strcmp(mode, "http") == 0) {
                ha_proxy_mode = HA_PROXY_MODE_HTTP;
            } else if (strcmp(mode, "health") == 0) {
                ha_proxy_mode = HA_PROXY_MODE_HEALTH;
            } else {
                ha_proxy_mode = HA_PROXY_MODE_UNKNOW;
            }
        }
    }

    label_set_add(&ha->labels, true, "proxy", fields[HA_STAT_PXNAME]);

    haproxy_field_t *ha_fields = NULL;
    size_t ha_fields_size = 0;

    switch(ha_type) {
    case HA_TYPE_FRONTEND:
        ha_fields = haproxy_frontend_fields;
        ha_fields_size = haproxy_frontend_fields_size;
        break;
    case HA_TYPE_BACKEND:
        ha_fields = haproxy_backend_fields;
        ha_fields_size = haproxy_backend_fields_size;
        break;
    case HA_TYPE_SERVER:
        ha_fields = haproxy_server_fields;
        ha_fields_size = haproxy_server_fields_size;
        label_set_add(&ha->labels, true, "server", fields[HA_STAT_SVNAME]);
        break;
    case HA_TYPE_LISTENER:
        ha_fields = haproxy_listener_fields;
        ha_fields_size = haproxy_listener_fields_size;
        label_set_add(&ha->labels, true, "listener", fields[HA_STAT_SVNAME]);
        break;
    }

    for(size_t i=0 ; i < ha_fields_size ; i++) {
        size_t n = ha_fields[i].field;
        if (n >= fields_len)
            continue;

        int field = stat_header[n];
         if (field == 0)
            continue;

        char *str = fields[field];
        if (*str == '\0')
            continue;

        metric_family_t *fam = &ha->fams_stat[ha_fields[i].fam];

        value_t value = {0};
        switch (fam->type) {
        case METRIC_TYPE_COUNTER:
            value = VALUE_COUNTER(atoll(str));
            break;
        case METRIC_TYPE_GAUGE:
            value = VALUE_GAUGE(atoll(str));
            break;
        case METRIC_TYPE_STATE_SET:
            break;
        default:
            continue;
            break;
        }

        switch (n) {
        case HA_STAT_STATUS:
            switch(ha_type) {
            case HA_TYPE_FRONTEND: {
                state_t states[] = {
                    { .name = "DOWN", .enabled = !strncmp("STOP", str, strlen("STOP"))},
                    { .name = "UP",   .enabled = !strncmp("OPEN", str, strlen("OPEN"))},
                };
                state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                metric_family_append(fam, VALUE_STATE_SET(set), &ha->labels, NULL);
            }   break;
            case HA_TYPE_BACKEND: {
                state_t states[] = {
                    { .name = "DOWN", .enabled = !strncmp("DOWN", str, strlen("DOWN")) },
                    { .name = "UP",   .enabled = !strncmp("UP", str, strlen("UP")) },
                };
                state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                metric_family_append(fam, VALUE_STATE_SET(set), &ha->labels, NULL);
            }   break;
            case HA_TYPE_SERVER: {
                state_t states[] = {
                    { .name = "DOWN",     .enabled = false },
                    { .name = "UP",       .enabled = false },
                    { .name = "MAINT",    .enabled = false },
                    { .name = "DRAIN",    .enabled = false },
                    { .name = "NOLB",     .enabled = false },
                    { .name = "no check", .enabled = false },
                };
                state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                for (size_t j = 0; j < set.num ; j++) {
                    if (strncmp(set.ptr[j].name, str, strlen(set.ptr[j].name)) == 0) {
                        set.ptr[j].enabled = true;
                        break;
                    }
                }
                metric_family_append(fam, VALUE_STATE_SET(set), &ha->labels, NULL);
            }   break;
            case HA_TYPE_LISTENER: {
                state_t states[] = {
                    { .name = "WAITING", .enabled = false },
                    { .name = "OPEN",    .enabled = false },
                    { .name = "FULL",    .enabled = false },
                };
                state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                for (size_t j = 0; j < set.num ; j++) {
                    if (strncmp(set.ptr[j].name, str, strlen(set.ptr[j].name)) == 0) {
                        set.ptr[j].enabled = true;
                        break;
                    }
                }
                metric_family_append(fam, VALUE_STATE_SET(set), &ha->labels, NULL);
            }   break;
            }
            break;
        case HA_STAT_CHECK_STATUS: {
            state_t states[] = {
                { .name = "UNK",     .enabled = false },
                { .name = "INI",     .enabled = false },
                { .name = "CHECKED", .enabled = false },
                { .name = "HANA",    .enabled = false },
                { .name = "SOCKERR", .enabled = false },
                { .name = "L4OK",    .enabled = false },
                { .name = "L4TOUT",  .enabled = false },
                { .name = "L4CON",   .enabled = false },
                { .name = "L6OK",    .enabled = false },
                { .name = "L6TOUT",  .enabled = false },
                { .name = "L6RSP",   .enabled = false },
                { .name = "L7TOUT",  .enabled = false },
                { .name = "L7RSP",   .enabled = false },
                { .name = "L7OKC",   .enabled = false },
                { .name = "L7OK",    .enabled = false },
                { .name = "L7STS",   .enabled = false },
                { .name = "PROCERR", .enabled = false },
                { .name = "PROCTOUT",.enabled = false },
                { .name = "PROCOK",  .enabled = false },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
            for (size_t j = 0; j < set.num ; j++) {
                if (strncmp(set.ptr[j].name, str, strlen(set.ptr[j].name)) == 0) { // FIXME
                    set.ptr[j].enabled = true;
                    break;
                }
            }
            metric_family_append(fam, VALUE_STATE_SET(set), &ha->labels, NULL);
        }   break;
        case HA_STAT_HRSP_1XX:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels,
                                     &(label_pair_const_t){.name="code", .value="1xx"}, NULL);
            break;
        case HA_STAT_HRSP_2XX:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels,
                                     &(label_pair_const_t){.name="code", .value="2xx"}, NULL);
            break;
        case HA_STAT_HRSP_3XX:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels,
                                     &(label_pair_const_t){.name="code", .value="3xx"}, NULL);
            break;
        case HA_STAT_HRSP_4XX:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels,
                                     &(label_pair_const_t){.name="code", .value="4xx"}, NULL);
            break;
        case HA_STAT_HRSP_5XX:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels,
                                     &(label_pair_const_t){.name="code", .value="5xx"}, NULL);
            break;
        case HA_STAT_HRSP_OTHER:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels,
                                     &(label_pair_const_t){.name="code", .value="other"}, NULL);
            break;
        case HA_STAT_REQ_RATE_MAX:
        case HA_STAT_REQ_TOT:
        case HA_STAT_INTERCEPTED:
        case HA_STAT_CACHE_LOOKUPS:
        case HA_STAT_CACHE_HITS:
        case HA_STAT_COMP_IN:
        case HA_STAT_COMP_OUT:
        case HA_STAT_COMP_BYP:
        case HA_STAT_COMP_RSP:
            if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
                metric_family_append(fam, value, &ha->labels, NULL);
            break;
        case HA_STAT_CHECK_DURATION:
        case HA_STAT_QTIME:
        case HA_STAT_CTIME:
        case HA_STAT_RTIME:
        case HA_STAT_TTIME:
        case HA_STAT_QT_MAX:
        case HA_STAT_CT_MAX:
        case HA_STAT_RT_MAX:
        case HA_STAT_TT_MAX:
            value.gauge.float64 /= 1000.0;
            metric_family_append(fam, value, &ha->labels, NULL);
            break;
        default:
            if ((fam->type == METRIC_TYPE_COUNTER) || (fam->type == METRIC_TYPE_GAUGE))
                metric_family_append(fam, value, &ha->labels, NULL);
            break;
        }
    }

    label_set_add(&ha->labels, true, "server", NULL);
    label_set_add(&ha->labels, true, "listener", NULL);
    label_set_add(&ha->labels, true, "proxy", NULL);

    return 0;
}

static int haproxy_read_curl_stat(haproxy_t *ha, cdtime_t when)
{
    if (ha->buffer_fill == 0)
        return 0;

    int stat_header[HA_STAT_MAX] = {0};
    bool stat_header_found = false;
    char *ptr = ha->buffer;
    char *saveptr = NULL;
    char *line;
    while ((line = strtok_r(ptr, "\n", &saveptr)) != NULL) {
        ptr = NULL;
        if (line[0] == '#') {
            if (!stat_header_found) {
                int cols = haproxy_read_stat_header(line, stat_header);
                if (cols > 0)
                    stat_header_found = true;
            }
        } else {
            if (stat_header_found)
                haproxy_read_stat_line(ha, stat_header, line);
        }
    }

    plugin_dispatch_metric_family_array_filtered(ha->fams_stat, FAM_HAPROXY_STAT_MAX,
                                                 ha->filter, when);

    return 0;
}

static int haproxy_read_cmd_stat(haproxy_t *ha)
{
    cdtime_t when = cdtime();
    FILE *fp = haproxy_cmd (ha, "show stat\n");
    if (fp == NULL)
        return -1;

    int stat_header[HA_STAT_MAX] = {0};
    bool stat_header_found = false;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        size_t len = strlen(buffer);
        while ((len > 0) && (buffer[len-1] == '\n')) {
            buffer[len-1] = '\0';
            len--;
        }

        if (*buffer == '\0')
            continue;

        if (buffer[0] == '#') {
            if (!stat_header_found) {
                int cols = haproxy_read_stat_header(buffer, stat_header);
                if (cols > 0)
                    stat_header_found = true;
            }
        } else {
            if (stat_header_found)
                haproxy_read_stat_line(ha, stat_header, buffer);
        }
    }

    plugin_dispatch_metric_family_array_filtered(ha->fams_stat, FAM_HAPROXY_STAT_MAX,
                                                 ha->filter, when);

    fclose(fp);
    return 0;
}

static int haproxy_read_cmd_info(haproxy_t *ha)
{
    cdtime_t when = cdtime();
    FILE *fp = haproxy_cmd (ha, "show info\n");
    if (fp == NULL)
        return -1;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        char *key = buffer;
        char *val = key;

        while ((*val != ':') && (*val != '\0')) val++;
        if (*val == '\0')
            continue;
        *val = '\0';
        val++;

        while (*val == ' ') val++;
        if (*val == '\0')
            continue;

        size_t len = strlen(val);
        while ((len > 0) && (val[len-1] == '\n')) {
            val[len-1] = '\0';
            len--;
        }

        const struct hainfo_metric *hm = hainfo_get_key (key, strlen(key));
        if (hm == NULL)
            continue;

        metric_family_t *fam = &ha->fams_process[hm->fam];
        value_t value = {0};
        if (hm->fam == FAM_HAPROXY_PROCESS_BUILD_INFO) {
            label_set_t info = {
                .num = 1,
                .ptr = (label_pair_t[]){
                    {.name = "version", .value = val}
                }
            };
            metric_family_append(fam, VALUE_INFO(info), &ha->labels, NULL);
        } else if (hm->fam == FAM_HAPROXY_PROCESS_TAINTED) {
            value = VALUE_GAUGE(strtol(val, NULL, 16));
            metric_family_append(fam, value, &ha->labels, NULL);
        } else if (hm->fam == FAM_HAPROXY_PROCESS_BOOTTIME_SECONDS) {
            value = VALUE_GAUGE(atoll(val) / 1000.0);
            metric_family_append(fam, value, &ha->labels, NULL);
        } else {
            if (fam->type == METRIC_TYPE_GAUGE)
                value = VALUE_GAUGE(atoll(val));
            else
                value = VALUE_COUNTER(atoll(val));

            metric_family_append(fam, value, &ha->labels, NULL);
        }
    }

    plugin_dispatch_metric_family_array_filtered(ha->fams_process, FAM_HAPROXY_PROCESS_MAX,
                                                 ha->filter, when);

    fclose(fp);
    return 0;
}

static int haproxy_read_cmd_table(haproxy_t *ha)
{
    cdtime_t when = cdtime();
    FILE *fp = haproxy_cmd (ha, "show table\n");
    if (fp == NULL)
        return -1;

    char *fields[16];
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (buffer[0] != '#')
            continue;

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 7)
            continue;

        char *table = fields[2];
        size_t table_len = strlen(table);
        if (table_len == 0)
            continue;

        if (table[table_len - 1] == ',')
            table[table_len-1] = '\0';

        if (strncmp(fields[5], "size:", strlen("size:")) == 0) {
            size_t size_len = strlen(fields[5]);
            if (fields[5][size_len-1] == ',')
                fields[5][size_len-1] = '\0';

            double value = 0;
            if (strtodouble(fields[5] + strlen("size:"), &value) == 0)
                metric_family_append(&ha->fams_sticktable[FAM_HAPROXY_STICKTABLE_SIZE],
                                     VALUE_GAUGE(value), &ha->labels,
                                     &(label_pair_const_t){.name="table", .value=table}, NULL);
        }

        if (strncmp(fields[6], "used:", strlen("used:")) == 0) {
            double value = 0;
            if (strtodouble(fields[6] + strlen("used:"), &value) == 0)
                metric_family_append(&ha->fams_sticktable[FAM_HAPROXY_STICKTABLE_USED],
                                     VALUE_GAUGE(value), &ha->labels,
                                     &(label_pair_const_t){.name="table", .value=table}, NULL);
        }
    }

    plugin_dispatch_metric_family_array_filtered(ha->fams_sticktable, FAM_HAPROXY_STICKTABLE_MAX,
                                                 ha->filter, when);

    fclose(fp);
    return 0;
}

static int haproxy_read (user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    haproxy_t *ha = ud->data;

    metric_family_t fam_up = {
        .name = "haproxy_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the haproxy server be reached"
    };

    int status = 0;

    if (ha->socketpath != NULL) {
        while (status == 0) {
            status = haproxy_read_cmd_info(ha);
            if (status < 0)
                break;
            status = haproxy_read_cmd_stat(ha);
            if (status < 0)
                break;
            status = haproxy_read_cmd_table(ha);

            break;
        }
    } else {
        ha->buffer_fill = 0;

        while (status == 0) {
            CURLcode rccode = curl_easy_setopt(ha->curl, CURLOPT_URL, ha->url);
            if (rccode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                             curl_easy_strerror(rccode));
                status = -1;
                break;
            }

            status = curl_easy_perform(ha->curl);
            if (status != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_perform failed with status %i: %s",
                            status, ha->curl_errbuf);
                status = -1;
                break;
            }

            long rcode = 0;
            cdtime_t when = cdtime();
            status = curl_easy_getinfo(ha->curl, CURLINFO_RESPONSE_CODE, &rcode);
            if (status != CURLE_OK) {
                PLUGIN_ERROR("Fetching response code failed with status %i: %s",
                            status, ha->curl_errbuf);
                status = -1;
                break;
            }

            if (rcode == 200)
                haproxy_read_curl_stat(ha, when);

            break;
        }
    }

    metric_family_append(&fam_up, VALUE_GAUGE(status == 0 ? 1 : 0), &ha->labels, NULL);
    plugin_dispatch_metric_family_filtered(&fam_up, ha->filter, 0);

    return 0;
}

static size_t haproxy_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    size_t len = size * nmemb;
    if (len == 0)
        return len;

    haproxy_t *ha = user_data;
    if (ha == NULL)
        return 0;

    if ((ha->buffer_fill + len) >= ha->buffer_size) {
        size_t temp_size = ha->buffer_fill + len + 1;
        char * temp = realloc(ha->buffer, temp_size);
        if (temp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return 0;
        }
        ha->buffer = temp;
        ha->buffer_size = temp_size;
    }

    memcpy(ha->buffer + ha->buffer_fill, (char *)buf, len);
    ha->buffer_fill += len;
    ha->buffer[ha->buffer_fill] = 0;

    return len;
}

static int haproxy_init_curl(haproxy_t *ha)
{
    ha->curl = curl_easy_init();
    if (ha->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(ha->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s", curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_WRITEFUNCTION, haproxy_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_WRITEDATA, ha);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s", curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s", curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_ERRORBUFFER, ha->curl_errbuf);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s", curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s", curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_IPRESOLVE failed: %s", curl_easy_strerror(rcode));
        return -1;
    }


    if (ha->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
        rcode = curl_easy_setopt(ha->curl, CURLOPT_USERNAME, ha->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ha->curl, CURLOPT_PASSWORD, (ha->pass == NULL) ? "" : ha->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#else
        size_t credentials_size = strlen(ha->user) + 2;
        if (ha->pass != NULL)
            credentials_size += strlen(ha->pass);

        ha->credentials = malloc(credentials_size);
        if (ha->credentials == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        snprintf(ha->credentials, credentials_size, "%s:%s", ha->user,
                         (ha->pass == NULL) ? "" : ha->pass);
        rcode = curl_easy_setopt(ha->curl, CURLOPT_USERPWD, ha->credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s", curl_easy_strerror(rcode));
            return -1;
        }

#endif

        if (ha->digest) {
            rcode = curl_easy_setopt(ha->curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
            if (rcode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPAUTH failed: %s",
                             curl_easy_strerror(rcode));
                return -1;
            }
        }
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_SSL_VERIFYPEER, (long)ha->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_SSL_VERIFYHOST, ha->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (ha->cacert != NULL) {
        rcode = curl_easy_setopt(ha->curl, CURLOPT_CAINFO, ha->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
    if (ha->headers != NULL) {
        rcode = curl_easy_setopt(ha->curl, CURLOPT_HTTPHEADER, ha->headers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    rcode = curl_easy_setopt(ha->curl, CURLOPT_TIMEOUT_MS,
                                       (long)CDTIME_T_TO_MS(plugin_get_interval()));
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    return 0;
}

static int haproxy_config_append_string(config_item_t *ci, const char *name,
                                        struct curl_slist **dest)
{
    struct curl_slist *temp = NULL;
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'%s' needs exactly one string argument.", name);
        return -1;
    }

    temp = curl_slist_append(*dest, ci->values[0].value.string);
    if (temp == NULL)
        return -1;

    *dest = temp;

    return 0;
}

static int haproxy_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'instance' blocks need exactly one string argument.");
        return -1;
    }

    haproxy_t *ha = calloc(1, sizeof(*ha));
    if (ha == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ha->fams_process, fams_haproxy_process,
                 FAM_HAPROXY_PROCESS_MAX * sizeof(fams_haproxy_process[0]));
    memcpy(ha->fams_stat, fams_haproxy_stat,
                 FAM_HAPROXY_STAT_MAX * sizeof(fams_haproxy_stat[0]));
    memcpy(ha->fams_sticktable, fams_haproxy_sticktable,
                 FAM_HAPROXY_STICKTABLE_MAX* sizeof(fams_haproxy_sticktable[0]));

    ha->digest = false;
    ha->verify_peer = true;
    ha->verify_host = true;

    ha->instance = strdup(ci->values[0].value.string);
    if (ha->instance == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(ha);
        return -1;
    }

    cdtime_t interval = 0;
    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &ha->url);
        } else if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &ha->socketpath);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &ha->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &ha->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &ha->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &ha->pass);
        } else if (strcasecmp("digest", child->key) == 0) {
            status = cf_util_get_boolean(child, &ha->digest);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &ha->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &ha->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &ha->cacert);
        } else if (strcasecmp("header", child->key) == 0) {
            status = haproxy_config_append_string(child, "Header", &ha->headers);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ha->labels);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ha->filter);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if ((ha->url == NULL) && (ha->socketpath == NULL)) {
        PLUGIN_WARNING("'URL' or 'SocketPath' missing in 'Instance' block.");
        status = -1;
    }

    if ((status == 0) && (ha->url != NULL))
        status = haproxy_init_curl(ha);

    if (status != 0) {
        haproxy_free(ha);
        return status;
    }

    label_set_add(&ha->labels, true, "instance", ha->instance);

    return plugin_register_complex_read("haproxy", ha->instance, haproxy_read, interval,
                                        &(user_data_t){.data = ha, .free_func = haproxy_free});
}

static int haproxy_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = haproxy_config_instance(child);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int haproxy_init(void)
{
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_config("haproxy", haproxy_config);
    plugin_register_init("haproxy", haproxy_init);
}
