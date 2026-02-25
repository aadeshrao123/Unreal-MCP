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

			ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
			if (ClientSocket.IsValid())
			{
				UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Client connected"));

				ClientSocket->SetNoDelay(true);
				ClientSocket->SetNonBlocking(true);
				int32 SocketBufferSize = 65536;
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
			uint8 Buffer[8192];
			int32 BytesRead = 0;

			if (ClientSocket->Recv(Buffer, sizeof(Buffer) - 1, BytesRead))
			{
				if (BytesRead == 0)
				{
					UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Client disconnected"));
					ClientSocket->Close();
					ClientSocket.Reset();
				}
				else
				{
					Buffer[BytesRead] = '\0';
					FString ReceivedText = UTF8_TO_TCHAR(Buffer);

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
							const uint8* DataToSend = (const uint8*)UTF8Response.Get();
							int32 TotalDataSize = UTF8Response.Length();
							int32 TotalBytesSent = 0;

							while (TotalBytesSent < TotalDataSize)
							{
								int32 BytesSent = 0;
								if (!ClientSocket->Send(
									DataToSend + TotalBytesSent,
									TotalDataSize - TotalBytesSent,
									BytesSent))
								{
									UE_LOG(LogTemp, Error,
										TEXT("UnrealMCP: Send failed after %d/%d bytes"),
										TotalBytesSent, TotalDataSize);
									ClientSocket->Close();
									ClientSocket.Reset();
									break;
								}
								TotalBytesSent += BytesSent;
							}
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Missing 'type' field in command"));
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Failed to parse JSON: %s"), *ReceivedText);
					}
				}
			}
			else
			{
				int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();

				if (LastError == SE_EWOULDBLOCK)
				{
					// No data available — this is normal for non-blocking sockets
				}
				else if (LastError == SE_EINTR)
				{
					// Interrupted, just retry next loop
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Client disconnected (error %d)"), LastError);
					ClientSocket->Close();
					ClientSocket.Reset();
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

