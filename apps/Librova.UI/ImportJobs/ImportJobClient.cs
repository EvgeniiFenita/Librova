using Google.Protobuf;
using Librova.V1;
using Librova.UI.PipeTransport;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ImportJobs;

internal sealed class ImportJobClient
{
    private readonly NamedPipeClient _pipeClient;

    public ImportJobClient(string pipePath)
    {
        _pipeClient = new NamedPipeClient(pipePath);
    }

    public async Task<ulong> StartImportAsync(
        ImportRequest importRequest,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        var request = new StartImportRequest
        {
            Import = importRequest
        };

        var response = await _pipeClient.CallAsync(
            PipeMethod.StartImport,
            request,
            StartImportResponse.Parser,
            timeout,
            cancellationToken).ConfigureAwait(false);

        return response.JobId;
    }

    public Task<GetImportJobSnapshotResponse> GetSnapshotAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.GetImportJobSnapshot,
            new GetImportJobSnapshotRequest { JobId = jobId },
            GetImportJobSnapshotResponse.Parser,
            timeout,
            cancellationToken);

    public Task<GetImportJobResultResponse> GetResultAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.GetImportJobResult,
            new GetImportJobResultRequest { JobId = jobId },
            GetImportJobResultResponse.Parser,
            timeout,
            cancellationToken);

    public Task<WaitImportJobResponse> WaitAsync(
        ulong jobId,
        TimeSpan timeout,
        TimeSpan waitTimeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.WaitImportJob,
            new WaitImportJobRequest
            {
                JobId = jobId,
                TimeoutMs = checked((uint)waitTimeout.TotalMilliseconds)
            },
            WaitImportJobResponse.Parser,
            timeout,
            cancellationToken);

    public Task<CancelImportJobResponse> CancelAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.CancelImportJob,
            new CancelImportJobRequest { JobId = jobId },
            CancelImportJobResponse.Parser,
            timeout,
            cancellationToken);

    public Task<RemoveImportJobResponse> RemoveAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.RemoveImportJob,
            new RemoveImportJobRequest { JobId = jobId },
            RemoveImportJobResponse.Parser,
            timeout,
            cancellationToken);
}
