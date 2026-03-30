using System;
using System.Buffers.Binary;
using System.IO;
using System.Text;

namespace LibriFlow.UI.PipeTransport;

internal enum PipeMethod : uint
{
    StartImport = 1,
    GetImportJobSnapshot = 2,
    GetImportJobResult = 3,
    WaitImportJob = 4,
    CancelImportJob = 5,
    RemoveImportJob = 6
}

internal enum PipeResponseStatus : uint
{
    Ok = 0,
    InvalidRequest = 1,
    UnknownMethod = 2,
    InternalError = 3
}

internal sealed class PipeRequestEnvelope
{
    public ulong RequestId { get; init; }
    public PipeMethod Method { get; init; }
    public string Payload { get; init; } = string.Empty;
}

internal sealed class PipeResponseEnvelope
{
    public ulong RequestId { get; init; }
    public PipeResponseStatus Status { get; init; }
    public string Payload { get; init; } = string.Empty;
    public string ErrorMessage { get; init; } = string.Empty;
}

internal sealed class PipeParseException : IOException
{
    public PipeParseException(string message)
        : base(message)
    {
    }
}

internal static class PipeProtocol
{
    private const uint RequestMagic = 0x4C465250;
    private const uint ResponseMagic = 0x4C465253;
    private const uint ProtocolVersion = 1;

    public static byte[] SerializeRequestEnvelope(PipeRequestEnvelope envelope)
    {
        using var stream = new MemoryStream();
        WriteUInt32(stream, RequestMagic);
        WriteUInt32(stream, ProtocolVersion);
        WriteUInt64(stream, envelope.RequestId);
        WriteUInt32(stream, (uint)envelope.Method);
        WriteString(stream, envelope.Payload);
        return stream.ToArray();
    }

    public static byte[] SerializeResponseEnvelope(PipeResponseEnvelope envelope)
    {
        using var stream = new MemoryStream();
        WriteUInt32(stream, ResponseMagic);
        WriteUInt32(stream, ProtocolVersion);
        WriteUInt64(stream, envelope.RequestId);
        WriteUInt32(stream, (uint)envelope.Status);
        WriteString(stream, envelope.Payload);
        WriteString(stream, envelope.ErrorMessage);
        return stream.ToArray();
    }

    public static PipeRequestEnvelope DeserializeRequestEnvelope(ReadOnlySpan<byte> bytes)
    {
        var reader = new PipeReader(bytes);

        var magic = reader.ReadUInt32();
        if (magic != RequestMagic)
        {
            throw new PipeParseException("Unsupported request envelope magic.");
        }

        var version = reader.ReadUInt32();
        if (version != ProtocolVersion)
        {
            throw new PipeParseException("Unsupported request envelope version.");
        }

        var requestId = reader.ReadUInt64();
        var methodValue = reader.ReadUInt32();
        if (!Enum.IsDefined(typeof(PipeMethod), methodValue))
        {
            throw new PipeParseException("Unknown pipe method.");
        }

        var payload = reader.ReadString();
        reader.EnsureFullyConsumed("Request envelope contains trailing bytes.");

        return new PipeRequestEnvelope
        {
            RequestId = requestId,
            Method = (PipeMethod)methodValue,
            Payload = payload
        };
    }

    public static PipeResponseEnvelope DeserializeResponseEnvelope(ReadOnlySpan<byte> bytes)
    {
        var reader = new PipeReader(bytes);

        var magic = reader.ReadUInt32();
        if (magic != ResponseMagic)
        {
            throw new PipeParseException("Unsupported response envelope magic.");
        }

        var version = reader.ReadUInt32();
        if (version != ProtocolVersion)
        {
            throw new PipeParseException("Unsupported response envelope version.");
        }

        var requestId = reader.ReadUInt64();
        var statusValue = reader.ReadUInt32();
        if (!Enum.IsDefined(typeof(PipeResponseStatus), statusValue))
        {
            throw new PipeParseException("Unknown response status.");
        }

        var payload = reader.ReadString();
        var errorMessage = reader.ReadString();
        reader.EnsureFullyConsumed("Response envelope contains trailing bytes.");

        return new PipeResponseEnvelope
        {
            RequestId = requestId,
            Status = (PipeResponseStatus)statusValue,
            Payload = payload,
            ErrorMessage = errorMessage
        };
    }

    private static void WriteUInt32(Stream stream, uint value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(buffer, value);
        stream.Write(buffer);
    }

    private static void WriteUInt64(Stream stream, ulong value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(ulong)];
        BinaryPrimitives.WriteUInt64LittleEndian(buffer, value);
        stream.Write(buffer);
    }

    private static void WriteString(Stream stream, string value)
    {
        var bytes = Encoding.UTF8.GetBytes(value);
        WriteUInt32(stream, checked((uint)bytes.Length));
        stream.Write(bytes);
    }

    private ref struct PipeReader
    {
        private ReadOnlySpan<byte> _bytes;
        private int _offset;

        public PipeReader(ReadOnlySpan<byte> bytes)
        {
            _bytes = bytes;
            _offset = 0;
        }

        public uint ReadUInt32()
        {
            const int size = sizeof(uint);
            if (_offset + size > _bytes.Length)
            {
                throw new PipeParseException("Unexpected end of message while reading uint32.");
            }

            var value = BinaryPrimitives.ReadUInt32LittleEndian(_bytes.Slice(_offset, size));
            _offset += size;
            return value;
        }

        public ulong ReadUInt64()
        {
            const int size = sizeof(ulong);
            if (_offset + size > _bytes.Length)
            {
                throw new PipeParseException("Unexpected end of message while reading uint64.");
            }

            var value = BinaryPrimitives.ReadUInt64LittleEndian(_bytes.Slice(_offset, size));
            _offset += size;
            return value;
        }

        public string ReadString()
        {
            var length = checked((int)ReadUInt32());
            if (_offset + length > _bytes.Length)
            {
                throw new PipeParseException("Unexpected end of message while reading string.");
            }

            var value = Encoding.UTF8.GetString(_bytes.Slice(_offset, length));
            _offset += length;
            return value;
        }

        public void EnsureFullyConsumed(string message)
        {
            if (_offset != _bytes.Length)
            {
                throw new PipeParseException(message);
            }
        }
    }
}
