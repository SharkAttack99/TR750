
#ifndef _DATA_PACKET_H 
#define _DATA_PACKET_H

VOID CheckSecurityResult(
	IN PULONG   pData,
	IN	RTMP_ADAPTER *pAdapter
	);
VOID RxPacket(
	IN UINT8   *pData,
	IN RTMP_ADAPTER *pAdapter,
	IN UINT32  pktlen
	);
VOID HandleRXVector(
	IN PUCHAR   pData,
	IN	RTMP_ADAPTER *pAdapter
	);
NTSTATUS GatherRxvRSSI(RTMP_ADAPTER *pAd);
NTSTATUS GetAvgRxvRSSI(RTMP_ADAPTER *pAd, RSSI_DATA  *RssiData);
NTSTATUS SetRxvRSSILimit(RTMP_ADAPTER *pAd, UINT32 Limit);
NTSTATUS SetupTxPacket(RTMP_ADAPTER *pAd, UCHAR *pData, UINT32 Length, UINT32 BufferNum);
NTSTATUS StopTx(RTMP_ADAPTER *pAd);
NTSTATUS StartTx(RTMP_ADAPTER *pAd, UINT32 TxRemained, UINT32 Pipe);
NTSTATUS StopRx0(RTMP_ADAPTER *pAd);
NTSTATUS StopRx1(RTMP_ADAPTER *pAd);
NTSTATUS StartRx0(RTMP_ADAPTER *pAd);
NTSTATUS StartRx1(RTMP_ADAPTER *pAd);
NTSTATUS RxHandler(RTMP_ADAPTER *pAd, UCHAR *pRxBuf, UINT32 pktlen, UINT32 RxNum);
BOOLEAN IsFwRspPkt(RTMP_ADAPTER *pAd, UCHAR *pRxBuf, UINT32 pktlen);
#endif //_DATA_PACKET_H