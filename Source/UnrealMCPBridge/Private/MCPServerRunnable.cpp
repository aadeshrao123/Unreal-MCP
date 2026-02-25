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
	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread starting..."));

	while (bRunning)
	{
		bool bPending = false;
		if (ListenerSocket->HasPendingConnection(bPending) && bPending)
		{
			UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client connection pending, accepting..."));

			ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
			if (ClientSocket.IsValid())
			{
				UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client connection accepted"));

				ClientSocket->SetNoDelay(true);
				int32 SocketBufferSize = 65536;
				ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
				ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);

				uint8 Buffer[8192];
				while (bRunning)
				{
					int32 BytesRead = 0;
					if (ClientSocket->Recv(Buffer, sizeof(Buffer) - 1, BytesRead))
					{
						if (BytesRead == 0)
						{
							UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client disconnected (zero bytes)"));
							break;
						}

						Buffer[BytesRead] = '\0';
						FString ReceivedText = UTF8_TO_TCHAR(Buffer);
						UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Received: %s"), *ReceivedText);

						TSharedPtr<FJsonObject> JsonObject;
						TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedText);

						if (FJsonSerializer::Deserialize(Reader, JsonObject))
						{
							FString CommandType;
							if (JsonObject->TryGetStringField(TEXT("type"), CommandType))
							{
								UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Executing command: %s"), *CommandType);

								FString Response = Bridge->ExecuteCommand(
									CommandType, JsonObject->GetObjectField(TEXT("params")));

								UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Command executed, response length: %d"),
									Response.Len());

								FString LogResponse = Response.Len() > 200
									? Response.Left(200) + TEXT("...")
									: Response;
								UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Sending response (%d bytes): %s"),
									Response.Len(), *LogResponse);

								FTCHARToUTF8 UTF8Response(*Response);
								const uint8* DataToSend = (const uint8*)UTF8Response.Get();
								int32 TotalDataSize = UTF8Response.Length();
								int32 TotalBytesSent = 0;
								bool bSuccess = true;

								// TCP may not send everything at once
								while (TotalBytesSent < TotalDataSize)
								{
									int32 BytesSent = 0;
									bool bSendResult = ClientSocket->Send(
										DataToSend + TotalBytesSent,
										TotalDataSize - TotalBytesSent,
										BytesSent);

									if (!bSendResult)
									{
										int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
										UE_LOG(LogTemp, Error,
											TEXT("MCPServerRunnable: Failed to send after %d/%d bytes - Error: %d"),
											TotalBytesSent, TotalDataSize, LastError);
										bSuccess = false;
										break;
									}

									TotalBytesSent += BytesSent;
									UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Sent %d bytes (%d/%d total)"),
										BytesSent, TotalBytesSent, TotalDataSize);
								}

								if (bSuccess)
								{
									UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Response sent successfully (%d bytes)"),
										TotalBytesSent);
								}
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Missing 'type' field in command"));
							}
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to parse JSON from: %s"),
								*ReceivedText);
						}
					}
					else
					{
						int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
						bool bShouldBreak = true;

						if (LastError == SE_EWOULDBLOCK)
						{
							UE_LOG(LogTemp, Verbose, TEXT("MCPServerRunnable: Socket would block, continuing..."));
							bShouldBreak = false;
							FPlatformProcess::Sleep(0.01f);
						}
						else if (LastError == SE_EINTR)
						{
							UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Socket read interrupted, continuing..."));
							bShouldBreak = false;
						}
						else
						{
							UE_LOG(LogTemp, Warning,
								TEXT("MCPServerRunnable: Client disconnected or error. Error code: %d"), LastError);
						}

						if (bShouldBreak)
						{
							break;
						}
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to accept client connection"));
			}
		}

		FPlatformProcess::Sleep(0.01f);
	}

	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread stopping"));
	return 0;
}

void FMCPServerRunnable::Stop()
{
	bRunning = false;
}

void FMCPServerRunnable::Exit()
{
}

void FMCPServerRunnable::HandleClientConnection(TSharedPtr<FSocket> InClientSocket)
{
	if (!InClientSocket.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Invalid client socket in HandleClientConnection"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Starting to handle client connection"));

	InClientSocket->SetNonBlocking(false);

	const int32 MaxBufferSize = 4096;
	uint8 Buffer[MaxBufferSize];
	FString MessageBuffer;

	while (bRunning && InClientSocket.IsValid())
	{
		bool bIsConnected = InClientSocket->GetConnectionState() == SCS_Connected;
		UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Socket connected: %s"),
			bIsConnected ? TEXT("true") : TEXT("false"));

		uint32 PendingDataSize = 0;
		bool bHasPendingData = InClientSocket->HasPendingData(PendingDataSize);
		UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: HasPendingData=%s, Size=%d"),
			bHasPendingData ? TEXT("true") : TEXT("false"), PendingDataSize);

		int32 BytesRead = 0;
		bool bReadSuccess = InClientSocket->Recv(Buffer, MaxBufferSize - 1, BytesRead, ESocketReceiveFlags::None);

		UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Recv - Success=%s, BytesRead=%d"),
			bReadSuccess ? TEXT("true") : TEXT("false"), BytesRead);

		if (BytesRead > 0)
		{
			FString HexData;
			for (int32 i = 0; i < FMath::Min(BytesRead, 50); ++i)
			{
				HexData += FString::Printf(TEXT("%02X "), Buffer[i]);
			}
			UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Raw hex (first 50 bytes): %s%s"),
				*HexData, BytesRead > 50 ? TEXT("...") : TEXT(""));

			Buffer[BytesRead] = 0;
			FString ReceivedData = UTF8_TO_TCHAR(Buffer);
			UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Received: '%s'"), *ReceivedData);

			MessageBuffer.Append(ReceivedData);

			if (MessageBuffer.Contains(TEXT("\n")))
			{
				TArray<FString> Messages;
				MessageBuffer.ParseIntoArray(Messages, TEXT("\n"), true);

				UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Found %d message(s)"), Messages.Num());

				for (int32 i = 0; i < Messages.Num() - 1; ++i)
				{
					UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Processing message %d: '%s'"),
						i + 1, *Messages[i]);
					ProcessMessage(InClientSocket, Messages[i]);
				}

				MessageBuffer = Messages.Last();
			}
		}
		else if (!bReadSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Connection closed - Error: %d"),
				(int32)ISocketSubsystem::Get()->GetLastErrorCode());
			break;
		}

		FPlatformProcess::Sleep(0.01f);
	}

	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Exited message receive loop"));
}

void FMCPServerRunnable::ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message)
{
	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Processing message: %s"), *Message);

	TSharedPtr<FJsonObject> JsonMessage;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

	if (!FJsonSerializer::Deserialize(Reader, JsonMessage) || !JsonMessage.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to parse message as JSON"));
		return;
	}

	FString CommandType;
	if (!JsonMessage->TryGetStringField(TEXT("command"), CommandType))
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Message missing 'command' field"));
		return;
	}

	TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
	if (JsonMessage->HasField(TEXT("params")))
	{
		TSharedPtr<FJsonValue> ParamsValue = JsonMessage->TryGetField(TEXT("params"));
		if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
		{
			Params = ParamsValue->AsObject();
		}
	}

	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Executing command: %s"), *CommandType);

	FString Response = Bridge->ExecuteCommand(CommandType, Params);
	Response += TEXT("\n");

	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Sending response (%d bytes): %s"),
		Response.Len(), *Response);

	FTCHARToUTF8 UTF8Response(*Response);
	const uint8* DataToSend = (const uint8*)UTF8Response.Get();
	int32 TotalDataSize = UTF8Response.Length();
	int32 TotalBytesSent = 0;

	// TCP may not send everything at once
	while (TotalBytesSent < TotalDataSize)
	{
		int32 BytesSent = 0;
		if (!Client->Send(DataToSend + TotalBytesSent, TotalDataSize - TotalBytesSent, BytesSent))
		{
			UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Failed to send after %d/%d bytes"),
				TotalBytesSent, TotalDataSize);
			return;
		}

		TotalBytesSent += BytesSent;
		UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Sent %d bytes (%d/%d total)"),
			BytesSent, TotalBytesSent, TotalDataSize);
	}

	UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Response sent successfully (%d bytes)"),
		TotalBytesSent);
}
