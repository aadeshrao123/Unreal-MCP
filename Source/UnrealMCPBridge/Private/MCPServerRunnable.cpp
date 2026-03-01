#include "MCPServerRunnable.h"
#include "EpicUnrealMCPBridge.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "HAL/PlatformProcess.h"

FMCPServerRunnable::FMCPServerRunnable(UEpicUnrealMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
	: Bridge(InBridge)
	, ListenerSocket(InListenerSocket)
	, bRunning(true)
{
	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Created server runnable"));
}

FMCPServerRunnable::~FMCPServerRunnable()
{
}

bool FMCPServerRunnable::Init()
{
	return true;
}

uint32 FMCPServerRunnable::Run()
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Server thread starting..."));

	while (bRunning)
	{
		bool bPending = false;
		if (ListenerSocket->HasPendingConnection(bPending) && bPending)
		{
			// If we already have a client, close it to make room for the new one.
			// The old client's Python side will reconnect automatically on its next command.
			if (ClientSocket.IsValid())
			{
				UE_LOG(LogTemp, Display, TEXT("UnrealMCP: New client connecting, closing previous connection"));
				ClientSocket->Close();
				ClientSocket.Reset();
			}
			ResetRecvState();

			ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
			if (ClientSocket.IsValid())
			{
				UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Client connected"));

				ClientSocket->SetNoDelay(true);
				ClientSocket->SetNonBlocking(true);
				int32 SocketBufferSize = 4 * 1024 * 1024;
				ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
				ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Failed to accept client connection"));
			}
		}

		// Service the current client (non-blocking)
		if (ClientSocket.IsValid())
		{
			// Read any available data into the accumulation buffer
			uint8 TempBuf[8192];
			int32 BytesRead = 0;

			if (ClientSocket->Recv(TempBuf, sizeof(TempBuf), BytesRead))
			{
				// Recv returns true with BytesRead > 0 when data arrives.
				// Recv returns true with BytesRead == 0 for EWOULDBLOCK on
				// streaming sockets (no data available — NOT a disconnect).
				// Graceful disconnect (recv returning 0) makes Recv return
				// false for streaming sockets, handled in the else branch.
				if (BytesRead > 0)
				{
					RecvBuffer.Append(TempBuf, BytesRead);
				}
			}
			else
			{
				int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();

				if (LastError == SE_EWOULDBLOCK)
				{
					// No data available — normal for non-blocking sockets
				}
				else if (LastError == SE_EINTR)
				{
					// Interrupted, just retry next loop
				}
				else
				{
					// Real error or graceful disconnect
					UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Client disconnected (error %d)"), LastError);
					ClientSocket->Close();
					ClientSocket.Reset();
					ResetRecvState();
				}
			}

			// Check if we have a complete length-prefixed message
			FString ReceivedText;
			if (TryExtractMessage(ReceivedText))
			{
				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedText);

				if (FJsonSerializer::Deserialize(Reader, JsonObject))
				{
					FString CommandType;
					if (JsonObject->TryGetStringField(TEXT("type"), CommandType))
					{
						UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Executing: %s"), *CommandType);

						FString Response = Bridge->ExecuteCommand(
							CommandType, JsonObject->GetObjectField(TEXT("params")));

						FTCHARToUTF8 UTF8Response(*Response);
						const uint8* JsonData = (const uint8*)UTF8Response.Get();
						int32 JsonSize = UTF8Response.Length();

						UE_LOG(LogTemp, Display,
							TEXT("UnrealMCP: Sending response for %s (%d bytes)"),
							*CommandType, JsonSize);

						// Length-prefix framing: 4-byte big-endian header + JSON payload
						uint8 LengthHeader[4];
						LengthHeader[0] = (uint8)((JsonSize >> 24) & 0xFF);
						LengthHeader[1] = (uint8)((JsonSize >> 16) & 0xFF);
						LengthHeader[2] = (uint8)((JsonSize >> 8) & 0xFF);
						LengthHeader[3] = (uint8)(JsonSize & 0xFF);

						if (SendAllBytes(LengthHeader, 4) && SendAllBytes(JsonData, JsonSize))
						{
							UE_LOG(LogTemp, Display,
								TEXT("UnrealMCP: Sent %d bytes for %s"),
								JsonSize, *CommandType);
						}
						else
						{
							UE_LOG(LogTemp, Error,
								TEXT("UnrealMCP: Failed to send response for %s"),
								*CommandType);
							ClientSocket->Close();
							ClientSocket.Reset();
							ResetRecvState();
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Missing 'type' field in command"));
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("UnrealMCP: Failed to parse JSON (%d chars)"),
						ReceivedText.Len());
					// Protocol is corrupted — reset and let Python reconnect
					ClientSocket->Close();
					ClientSocket.Reset();
					ResetRecvState();
				}
			}
		}

		FPlatformProcess::Sleep(0.002f);
	}

	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Server thread stopping"));
	return 0;
}

void FMCPServerRunnable::Stop()
{
	bRunning = false;
}

void FMCPServerRunnable::Exit()
{
}

void FMCPServerRunnable::ResetRecvState()
{
	RecvBuffer.Reset();
	RecvBufferReadOffset = 0;
}

bool FMCPServerRunnable::TryExtractMessage(FString& OutMessage)
{
	const int32 Available = RecvBuffer.Num() - RecvBufferReadOffset;

	// Need at least the 4-byte length header
	if (Available < 4)
	{
		return false;
	}

	// Read big-endian payload length from the header
	const uint8* HeaderPtr = RecvBuffer.GetData() + RecvBufferReadOffset;
	const int32 PayloadSize =
		(static_cast<int32>(HeaderPtr[0]) << 24) |
		(static_cast<int32>(HeaderPtr[1]) << 16) |
		(static_cast<int32>(HeaderPtr[2]) << 8) |
		static_cast<int32>(HeaderPtr[3]);

	// Sanity check — reject obviously corrupt headers
	if (PayloadSize <= 0 || PayloadSize > 10 * 1024 * 1024)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UnrealMCP: Invalid payload size %d — resetting connection"),
			PayloadSize);
		// Force disconnect so the client reconnects with a clean stream
		ClientSocket->Close();
		ClientSocket.Reset();
		ResetRecvState();
		return false;
	}

	// Check if the full payload has arrived
	if (Available < 4 + PayloadSize)
	{
		return false;
	}

	// Extract the payload as a UTF-8 string
	const uint8* PayloadPtr = HeaderPtr + 4;
	FUTF8ToTCHAR Converter(
		reinterpret_cast<const ANSICHAR*>(PayloadPtr), PayloadSize);
	OutMessage = FString(Converter.Length(), Converter.Get());

	// Advance past the consumed header + payload
	RecvBufferReadOffset += 4 + PayloadSize;

	// Compact the buffer once the consumed portion is large enough
	if (RecvBufferReadOffset > 4096)
	{
		RecvBuffer.RemoveAt(0, RecvBufferReadOffset);
		RecvBufferReadOffset = 0;
	}

	return true;
}

bool FMCPServerRunnable::SendAllBytes(const uint8* Data, int32 Size)
{
	int32 TotalSent = 0;
	int32 StallCount = 0;
	constexpr int32 MaxStalls = 500; // ~5 seconds max wait

	while (TotalSent < Size)
	{
		int32 BytesSent = 0;
		bool bOk = ClientSocket->Send(Data + TotalSent, Size - TotalSent, BytesSent);

		if (bOk && BytesSent > 0)
		{
			TotalSent += BytesSent;
			StallCount = 0;
		}
		else
		{
			int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
			if (LastError == SE_EWOULDBLOCK || BytesSent == 0)
			{
				if (++StallCount >= MaxStalls)
				{
					UE_LOG(LogTemp, Error,
						TEXT("UnrealMCP: Send stalled after %d/%d bytes"),
						TotalSent, Size);
					return false;
				}
				FPlatformProcess::Sleep(0.01f);
				continue;
			}

			UE_LOG(LogTemp, Error,
				TEXT("UnrealMCP: Send failed (error %d) after %d/%d bytes"),
				LastError, TotalSent, Size);
			return false;
		}
	}
	return true;
}
