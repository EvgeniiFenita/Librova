using LibriFlow.UI.PipeTransport;
using Xunit;

namespace LibriFlow.UI.Tests;

public sealed class PipeProtocolTests
{
    [Fact]
    public void RequestEnvelope_RoundTripsThroughBinaryFraming()
    {
        var envelope = new PipeRequestEnvelope
        {
            RequestId = 42,
            Method = PipeMethod.WaitImportJob,
            Payload = [1, 2, 3, 4]
        };

        var bytes = PipeProtocol.SerializeRequestEnvelope(envelope);
        var parsed = PipeProtocol.DeserializeRequestEnvelope(bytes);

        Assert.Equal(envelope.RequestId, parsed.RequestId);
        Assert.Equal(envelope.Method, parsed.Method);
        Assert.Equal(envelope.Payload, parsed.Payload);
    }

    [Fact]
    public void ResponseEnvelope_RoundTripsThroughBinaryFraming()
    {
        var envelope = new PipeResponseEnvelope
        {
            RequestId = 77,
            Status = PipeResponseStatus.InvalidRequest,
            Payload = [9, 8, 7],
            ErrorMessage = "broken payload"
        };

        var bytes = PipeProtocol.SerializeResponseEnvelope(envelope);
        var parsed = PipeProtocol.DeserializeResponseEnvelope(bytes);

        Assert.Equal(envelope.RequestId, parsed.RequestId);
        Assert.Equal(envelope.Status, parsed.Status);
        Assert.Equal(envelope.Payload, parsed.Payload);
        Assert.Equal(envelope.ErrorMessage, parsed.ErrorMessage);
    }

    [Fact]
    public void RequestEnvelope_RejectsUnknownMethodValues()
    {
        var bytes = PipeProtocol.SerializeRequestEnvelope(new PipeRequestEnvelope
        {
            RequestId = 7,
            Method = PipeMethod.StartImport
        });

        bytes[16] = 0xFF;
        bytes[17] = 0xFF;
        bytes[18] = 0xFF;
        bytes[19] = 0x7F;

        var error = Assert.Throws<PipeParseException>(() => PipeProtocol.DeserializeRequestEnvelope(bytes));
        Assert.Contains("Unknown pipe method", error.Message, StringComparison.Ordinal);
    }
}
