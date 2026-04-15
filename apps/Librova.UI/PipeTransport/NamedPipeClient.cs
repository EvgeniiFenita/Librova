using Google.Protobuf;
using Librova.UI.Logging;
using System;
using System.Buffers.Binary;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.PipeTransport;

internal sealed class PipeTransportException : IOException
{
    public PipeTransportException(string message)
        : base(message)
    {
    }
}

internal sealed class NamedPipeClient
{
    internal const int MaxPipeFrameBytes = 8 * 1024 * 1024;
    private static readonly TimeSpan SlowCallThreshold = TimeSpan.FromMilliseconds(500);
    private static ulong _nextRequestId = 1;
    private readonly string _pipePath;

    public NamedPipeClient(string pipePath)
    {
        _pipePath = pipePath;
    }

    public async Task<TResponse> CallAsync<TRequest, TResponse>(
        PipeMethod method,
        TRequest request,
        MessageParser<TResponse> responseParser,
        TimeSpan timeout,
        CancellationToken cancellationToken)
        where TRequest : class, IMessage<TRequest>
        where TResponse : class, IMessage<TResponse>
    {
        using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeoutCts.CancelAfter(timeout);

        await using var pipe = new NamedPipeClientStream(
            ".",
            ExtractPipeName(_pipePath),
            PipeDirection.InOut,
            PipeOptions.Asynchronous);

        try
        {
            await pipe.ConnectAsync((int)timeout.TotalMilliseconds, timeoutCts.Token).ConfigureAwait(false);
        }
        catch (TimeoutException)
        {
            UiLogging.Warning(
                "IPC timeout. Method={Method} TimeoutMs={TimeoutMs}",
                method,
                (int)timeout.TotalMilliseconds);
            throw new PipeTransportException("Timed out waiting for named pipe server.");
        }

        var envelope = new PipeRequestEnvelope
        {
            RequestId = NextRequestId(),
            Method = method,
            Payload = request.ToByteArray()
        };
        var stopwatch = Stopwatch.StartNew();

        await WriteFramedMessageAsync(pipe, PipeProtocol.SerializeRequestEnvelope(envelope), timeoutCts.Token)
            .ConfigureAwait(false);

        var responseBytes = await ReadFramedMessageAsync(pipe, timeoutCts.Token).ConfigureAwait(false);
        var responseEnvelope = PipeProtocol.DeserializeResponseEnvelope(responseBytes);

        if (responseEnvelope.RequestId != envelope.RequestId)
        {
            UiLogging.Warning(
                "IPC: mismatched response id. Method={Method} RequestId={RequestId} Expected={Expected} Received={Received}",
                method,
                envelope.RequestId,
                envelope.RequestId,
                responseEnvelope.RequestId);
            throw new PipeTransportException("Received mismatched pipe response id.");
        }

        if (responseEnvelope.Status != PipeResponseStatus.Ok)
        {
            UiLogging.Warning(
                "IPC: non-OK response. Method={Method} RequestId={RequestId} Status={Status} Error={Error} ElapsedMs={ElapsedMs}",
                method,
                envelope.RequestId,
                responseEnvelope.Status,
                responseEnvelope.ErrorMessage,
                stopwatch.ElapsedMilliseconds);
            throw new PipeTransportException(
                string.IsNullOrWhiteSpace(responseEnvelope.ErrorMessage)
                    ? "Pipe request failed."
                    : responseEnvelope.ErrorMessage);
        }

        if (ShouldLogSuccessfulCallAtInformation(method, stopwatch.Elapsed))
        {
            UiLogging.Information(
                "IPC call completed. Method={Method} RequestId={RequestId} ElapsedMs={ElapsedMs} Status=Ok",
                method,
                envelope.RequestId,
                stopwatch.ElapsedMilliseconds);
        }
        else
        {
            UiLogging.Debug(
                "IPC call completed. Method={Method} RequestId={RequestId} ElapsedMs={ElapsedMs} Status=Ok",
                method,
                envelope.RequestId,
                stopwatch.ElapsedMilliseconds);
        }

        return responseParser.ParseFrom(responseEnvelope.Payload);
    }

    private static async Task WriteFramedMessageAsync(
        PipeStream pipe,
        byte[] message,
        CancellationToken cancellationToken)
    {
        if (message.Length > MaxPipeFrameBytes)
        {
            throw new PipeTransportException("Named pipe frame exceeds the configured maximum size.");
        }

        var prefix = new byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(prefix, checked((uint)message.Length));
        await pipe.WriteAsync(prefix, cancellationToken).ConfigureAwait(false);
        if (message.Length != 0)
        {
            await pipe.WriteAsync(message, cancellationToken).ConfigureAwait(false);
        }

        await pipe.FlushAsync(cancellationToken).ConfigureAwait(false);
    }

    private static async Task<byte[]> ReadFramedMessageAsync(PipeStream pipe, CancellationToken cancellationToken)
    {
        var prefix = await ReadExactAsync(pipe, sizeof(uint), cancellationToken).ConfigureAwait(false);
        var length = BinaryPrimitives.ReadUInt32LittleEndian(prefix);
        if (length == 0)
        {
            return [];
        }

        if (length > MaxPipeFrameBytes)
        {
            throw new PipeTransportException("Named pipe frame exceeds the configured maximum size.");
        }

        return await ReadExactAsync(pipe, checked((int)length), cancellationToken).ConfigureAwait(false);
    }

    private static async Task<byte[]> ReadExactAsync(PipeStream pipe, int length, CancellationToken cancellationToken)
    {
        var buffer = new byte[length];
        var offset = 0;

        while (offset < length)
        {
            var read = await pipe.ReadAsync(buffer.AsMemory(offset, length - offset), cancellationToken)
                .ConfigureAwait(false);
            if (read == 0)
            {
                throw new PipeTransportException("Named pipe closed while reading a framed message.");
            }

            offset += read;
        }

        return buffer;
    }

    private static string ExtractPipeName(string pipePath)
    {
        const string prefix = @"\\.\pipe\";
        if (!pipePath.StartsWith(prefix, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("Pipe path must use the Windows named pipe syntax.");
        }

        return pipePath[prefix.Length..];
    }

    private static bool ShouldLogSuccessfulCallAtInformation(PipeMethod method, TimeSpan elapsed) =>
        elapsed >= SlowCallThreshold
        || method is PipeMethod.StartImport
            or PipeMethod.ValidateImportSources
            or PipeMethod.GetImportJobResult
            or PipeMethod.CancelImportJob
            or PipeMethod.RemoveImportJob;

    private static ulong NextRequestId() => System.Threading.Interlocked.Increment(ref _nextRequestId);
}
