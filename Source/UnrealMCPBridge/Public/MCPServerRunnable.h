#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "Interfaces/IPv4/IPv4Address.h"

class UEpicUnrealMCPBridge;

/**
 * Runnable class for the MCP server thread
 */
class FMCPServerRunnable : public FRunnable
{
public:
	FMCPServerRunnable(UEpicUnrealMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket);
	virtual ~FMCPServerRunnable();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	/** Send all bytes with EWOULDBLOCK retry. Returns false on fatal error. */
	bool SendAllBytes(const uint8* Data, int32 Size);

	/**
	 * Try to extract a complete length-prefixed message from RecvBuffer.
	 * Returns true and fills OutMessage if a complete message is available.
	 * The consumed bytes are removed from RecvBuffer.
	 */
	bool TryExtractMessage(FString& OutMessage);

	/** Reset receive state (on disconnect or protocol error). */
	void ResetRecvState();

	UEpicUnrealMCPBridge* Bridge;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ClientSocket;
	bool bRunning;

	// Message accumulation buffer for length-prefix framed requests.
	// Data is appended by non-blocking Recv calls and consumed when
	// a complete message (4-byte header + payload) is available.
	TArray<uint8> RecvBuffer;
	int32 RecvBufferReadOffset = 0;
};