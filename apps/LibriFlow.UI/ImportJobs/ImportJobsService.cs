using System;
using System.Threading;
using System.Threading.Tasks;

namespace LibriFlow.UI.ImportJobs;

internal sealed class ImportJobsService
{
    private readonly ImportJobClient _client;

    public ImportJobsService(string pipePath)
        : this(new ImportJobClient(pipePath))
    {
    }

    public ImportJobsService(ImportJobClient client)
    {
        _client = client;
    }

    public Task<ulong> StartAsync(
        StartImportRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _client.StartImportAsync(ImportJobMapper.ToProto(request), timeout, cancellationToken);

    public async Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        var response = await _client.GetSnapshotAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
        return ImportJobMapper.FromProto(response);
    }

    public async Task<ImportJobResultModel?> TryGetResultAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        var response = await _client.GetResultAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
        return ImportJobMapper.FromProto(response);
    }

    public async Task<bool> WaitAsync(
        ulong jobId,
        TimeSpan timeout,
        TimeSpan waitTimeout,
        CancellationToken cancellationToken)
    {
        var response = await _client.WaitAsync(jobId, timeout, waitTimeout, cancellationToken).ConfigureAwait(false);
        return response.Completed;
    }

    public async Task<bool> CancelAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        var response = await _client.CancelAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
        return response.Accepted;
    }

    public async Task<bool> RemoveAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        var response = await _client.RemoveAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
        return response.Removed;
    }
}
