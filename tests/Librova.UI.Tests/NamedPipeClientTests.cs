using Google.Protobuf;
using Librova.UI.PipeTransport;
using Librova.UI.Logging;
using System.Buffers.Binary;
using System.IO.Pipes;
using Xunit;

namespace Librova.UI.Tests;

[Collection(UiLoggingCollection.Name)]
public sealed class NamedPipeClientTests
{
    [Fact]
    public async Task CallAsync_RejectsOversizedFramedResponse()
    {
        var pipeName = $"Librova.UI.NamedPipeClient.Tests.{Environment.ProcessId}.{Environment.TickCount64}";
        var pipePath = $@"\\.\pipe\{pipeName}";

        await using var server = new NamedPipeServerStream(
            pipeName,
            PipeDirection.InOut,
            1,
            PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous);

        var serverTask = Task.Run(async () =>
        {
            await server.WaitForConnectionAsync();
            await ReadClientFrameAsync(server);

            var prefix = new byte[sizeof(uint)];
            BinaryPrimitives.WriteUInt32LittleEndian(prefix, checked((uint)NamedPipeClient.MaxPipeFrameBytes + 1U));
            await server.WriteAsync(prefix);
            await server.FlushAsync();
        });

        var client = new NamedPipeClient(pipePath);
        var error = await Assert.ThrowsAsync<PipeTransportException>(() =>
            client.CallAsync(
                PipeMethod.WaitImportJob,
                new Librova.V1.WaitImportJobRequest { JobId = 1, TimeoutMs = 1 },
                Librova.V1.WaitImportJobResponse.Parser,
                TimeSpan.FromSeconds(5),
                CancellationToken.None));

        Assert.Contains("maximum size", error.Message, StringComparison.OrdinalIgnoreCase);
        await serverTask;
    }

    [Fact]
    public async Task CallAsync_LogsRequestIdAndElapsedTimeForSuccessfulResponse()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-named-pipe-client-tests",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);
        var logFilePath = Path.Combine(sandboxRoot, "ui.log");

        var pipeName = $"Librova.UI.NamedPipeClient.Tests.{Environment.ProcessId}.{Environment.TickCount64}";
        var pipePath = $@"\\.\pipe\{pipeName}";

        await using var server = new NamedPipeServerStream(
            pipeName,
            PipeDirection.InOut,
            1,
            PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous);

        try
        {
            UiLogging.ReinitializeForTests(logFilePath);

            var serverTask = Task.Run(async () =>
            {
                await server.WaitForConnectionAsync();
                var requestBytes = await ReadClientFrameAsync(server);
                var requestEnvelope = PipeProtocol.DeserializeRequestEnvelope(requestBytes);

                var responseBytes = PipeProtocol.SerializeResponseEnvelope(new PipeResponseEnvelope
                {
                    RequestId = requestEnvelope.RequestId,
                    Status = PipeResponseStatus.Ok,
                    Payload = new Librova.V1.RemoveImportJobResponse { Removed = true }.ToByteArray()
                });

                await WriteServerFrameAsync(server, responseBytes);
            });

            var client = new NamedPipeClient(pipePath);
            var response = await client.CallAsync(
                PipeMethod.RemoveImportJob,
                new Librova.V1.RemoveImportJobRequest { JobId = 1 },
                Librova.V1.RemoveImportJobResponse.Parser,
                TimeSpan.FromSeconds(5),
                CancellationToken.None);

            UiLogging.Shutdown();
            var logText = await File.ReadAllTextAsync(logFilePath);

            Assert.True(response.Removed);
            Assert.Contains("IPC call completed.", logText, StringComparison.Ordinal);
            Assert.Contains("RemoveImportJob", logText, StringComparison.Ordinal);
            Assert.Contains("RequestId=", logText, StringComparison.Ordinal);
            Assert.Contains("ElapsedMs=", logText, StringComparison.Ordinal);
            Assert.Contains("Status=Ok", logText, StringComparison.Ordinal);

            await serverTask;
        }
        finally
        {
            UiLogging.Shutdown();

            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    private static async Task<byte[]> ReadClientFrameAsync(PipeStream stream)
    {
        var prefix = new byte[sizeof(uint)];
        await ReadExactAsync(stream, prefix);
        var length = BinaryPrimitives.ReadUInt32LittleEndian(prefix);
        if (length == 0)
        {
            return [];
        }

        var payload = new byte[length];
        await ReadExactAsync(stream, payload);
        return payload;
    }

    private static async Task WriteServerFrameAsync(PipeStream stream, byte[] payload)
    {
        var prefix = new byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(prefix, checked((uint)payload.Length));
        await stream.WriteAsync(prefix);
        if (payload.Length != 0)
        {
            await stream.WriteAsync(payload);
        }

        await stream.FlushAsync();
    }

    private static async Task ReadExactAsync(PipeStream stream, byte[] buffer)
    {
        var offset = 0;
        while (offset < buffer.Length)
        {
            var read = await stream.ReadAsync(buffer.AsMemory(offset, buffer.Length - offset));
            if (read == 0)
            {
                throw new IOException("Named pipe closed while reading test payload.");
            }

            offset += read;
        }
    }
}
