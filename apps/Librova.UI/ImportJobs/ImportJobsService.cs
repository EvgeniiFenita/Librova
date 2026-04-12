using System;
using System.Collections.Generic;
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
            "Starting import job request. SourceCount={SourceCount} WorkingDirectory={WorkingDirectory} TimeoutMs={TimeoutMs}",
            request.SourcePaths.Count,
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
            var response = await _client.GetSnapshotAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            var result = ImportJobMapper.FromProto(response);
            UiLogging.Debug(
                "TryGetSnapshotAsync completed. JobId={JobId} Found={Found} Status={Status}",
                jobId,
                result is not null,
                result?.Status.ToString() ?? "<none>");
            return result;
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
            var response = await _client.GetResultAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            var result = ImportJobMapper.FromProto(response);
            UiLogging.Information(
                "TryGetResultAsync completed. JobId={JobId} Found={Found} Status={Status}",
                jobId,
                result is not null,
                result?.Snapshot.Status.ToString() ?? "<none>");
            return result;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to get import job result. JobId={JobId}", jobId);
            throw;
        }
    }

    public async Task<string> ValidateSourcesAsync(
        IReadOnlyList<string> sourcePaths,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.ValidateSourcesAsync(sourcePaths, timeout, cancellationToken).ConfigureAwait(false);
            var blockingMessage = response.HasBlockingMessage ? response.BlockingMessage : string.Empty;
            UiLogging.Information(
                "ValidateSourcesAsync completed. SourceCount={SourceCount} Blocking={Blocking}",
                sourcePaths.Count,
                blockingMessage.Length == 0 ? "<none>" : blockingMessage);
            return blockingMessage;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to validate import sources. SourceCount={SourceCount}", sourcePaths.Count);
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
            var response = await _client.WaitAsync(jobId, timeout, waitTimeout, cancellationToken).ConfigureAwait(false);
            if (response.Completed)
            {
                UiLogging.Information(
                    "WaitAsync completed. JobId={JobId} Completed=true WaitTimeoutMs={WaitTimeoutMs}",
                    jobId,
                    waitTimeout.TotalMilliseconds);
            }
            else
            {
                UiLogging.Debug(
                    "WaitAsync completed. JobId={JobId} Completed=false WaitTimeoutMs={WaitTimeoutMs}",
                    jobId,
                    waitTimeout.TotalMilliseconds);
            }
            return response.Completed;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed while waiting for import job completion. JobId={JobId}", jobId);
            throw;
        }
    }

    public async Task<ImportJobResultModel> WaitForCompletionAsync(
        ulong jobId,
        TimeSpan timeout,
        TimeSpan waitTimeout,
        Action<ImportJobSnapshotModel>? onProgress,
        CancellationToken cancellationToken)
    {
        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var completed = await WaitAsync(jobId, timeout, waitTimeout, cancellationToken).ConfigureAwait(false);
            if (completed)
            {
                var result = await TryGetResultAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
                if (result is null)
                {
                    throw new InvalidOperationException(
                        $"Import job {jobId} reported completion but returned no terminal result.");
                }

                return result;
            }

            var snapshot = await TryGetSnapshotAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            if (snapshot is not null)
            {
                onProgress?.Invoke(snapshot);
            }
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
            var accepted = ImportJobMapper.FromProto(response);
            UiLogging.Information("CancelAsync completed. JobId={JobId} Accepted={Accepted}", jobId, accepted);
            return accepted;
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
            var removed = ImportJobMapper.FromProto(response);
            UiLogging.Information("RemoveAsync completed. JobId={JobId} Removed={Removed}", jobId, removed);
            return removed;
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
            UiLogging.Error(error, "Failed to start import job. SourceCount={SourceCount}", request.SourcePaths.Count);
            throw;
        }
    }
}
