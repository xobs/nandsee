#ifndef BYTESWAP_H
#define BYTESWAP_H

#include <QtGlobal>

#ifdef __cplusplus
extern "C" {
#endif

quint16 _htons(quint16 x);
quint16 _ntohs(quint16 x);
quint32 _htonl(quint32 x);
quint32 _ntohl(quint32 x);

#ifdef __cplusplus
};
#endif

#endif // BYTESWAP_H
