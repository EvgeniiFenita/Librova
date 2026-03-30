using Librova.UI.PipeTransport;
using System.Buffers.Binary;
using System.IO.Pipes;
using Xunit;

namespace Librova.UI.Tests;

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

    private static async Task ReadClientFrameAsync(PipeStream stream)
    {
        var prefix = new byte[sizeof(uint)];
        await ReadExactAsync(stream, prefix);
        var length = BinaryPrimitives.ReadUInt32LittleEndian(prefix);
        if (length == 0)
        {
            return;
        }

        await ReadExactAsync(stream, new byte[length]);
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
