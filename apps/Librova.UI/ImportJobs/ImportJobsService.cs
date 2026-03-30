using System;
using System.Threading;
using System.Threading.Tasks;
using Librova.UI.Logging;

namespace Librova.UI.ImportJobs;

internal sealed class ImportJobsService : IImportJobsService
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
        CancellationToken cancellationToken)
    {
        UiLogging.Information(
            "Starting import job request. SourcePath={SourcePath} WorkingDirectory={WorkingDirectory} TimeoutMs={TimeoutMs}",
            request.SourcePath,
            request.WorkingDirectory,
            timeout.TotalMilliseconds);

        return StartCoreAsync(request, timeout, cancellationToken);
    }

    public async Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            UiLogging.Information("Requesting import job snapshot. JobId={JobId} TimeoutMs={TimeoutMs}", jobId, timeout.TotalMilliseconds);
            var response = await _client.GetSnapshotAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            return ImportJobMapper.FromProto(response);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to get import job snapshot. JobId={JobId}", jobId);
            throw;
        }
    }

    public async Task<ImportJobResultModel?> TryGetResultAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            UiLogging.Information("Requesting import job result. JobId={JobId} TimeoutMs={TimeoutMs}", jobId, timeout.TotalMilliseconds);
            var response = await _client.GetResultAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            return ImportJobMapper.FromProto(response);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to get import job result. JobId={JobId}", jobId);
            throw;
        }
    }

    public async Task<bool> WaitAsync(
        ulong jobId,
        TimeSpan timeout,
        TimeSpan waitTimeout,
        CancellationToken cancellationToken)
    {
        try
        {
            UiLogging.Information(
                "Waiting for import job completion. JobId={JobId} TimeoutMs={TimeoutMs} WaitTimeoutMs={WaitTimeoutMs}",
                jobId,
                timeout.TotalMilliseconds,
                waitTimeout.TotalMilliseconds);
            var response = await _client.WaitAsync(jobId, timeout, waitTimeout, cancellationToken).ConfigureAwait(false);
            return response.Completed;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed while waiting for import job completion. JobId={JobId}", jobId);
            throw;
        }
    }

    public async Task<bool> CancelAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            UiLogging.Warning("Cancelling import job. JobId={JobId} TimeoutMs={TimeoutMs}", jobId, timeout.TotalMilliseconds);
            var response = await _client.CancelAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            return response.Accepted;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to cancel import job. JobId={JobId}", jobId);
            throw;
        }
    }

    public async Task<bool> RemoveAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            UiLogging.Information("Removing import job. JobId={JobId} TimeoutMs={TimeoutMs}", jobId, timeout.TotalMilliseconds);
            var response = await _client.RemoveAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            return response.Removed;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to remove import job. JobId={JobId}", jobId);
            throw;
        }
    }

    private async Task<ulong> StartCoreAsync(
        StartImportRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var jobId = await _client.StartImportAsync(
                ImportJobMapper.ToProto(request),
                timeout,
                cancellationToken).ConfigureAwait(false);

            UiLogging.Information("Import job started. JobId={JobId}", jobId);
            return jobId;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to start import job. SourcePath={SourcePath}", request.SourcePath);
            throw;
        }
    }
}
