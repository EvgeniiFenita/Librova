using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using Librova.UI.Logging;

namespace Librova.UI.ImportJobs;

internal sealed class ImportJobsService : IImportJobsService
{
    private static readonly TimeSpan RollingBackWaitTimeout = TimeSpan.FromSeconds(1);
    private static readonly TimeSpan CompactingWaitTimeout = TimeSpan.FromSeconds(2);
    private const int PercentLogStep = 10;
    private const ulong ProcessedEntriesLogStep = 25;

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
            var stopwatch = Stopwatch.StartNew();
            var response = await _client.GetSnapshotAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            var result = ImportJobMapper.FromProto(response);
            UiLogging.Debug(
                "TryGetSnapshotAsync completed. JobId={JobId} Found={Found} Status={Status} ElapsedMs={ElapsedMs}",
                jobId,
                result is not null,
                result?.Status.ToString() ?? "<none>",
                stopwatch.ElapsedMilliseconds);
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
            var stopwatch = Stopwatch.StartNew();
            var response = await _client.GetResultAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            var result = ImportJobMapper.FromProto(response);
            UiLogging.Information(
                "TryGetResultAsync completed. JobId={JobId} Found={Found} Status={Status} ElapsedMs={ElapsedMs}",
                jobId,
                result is not null,
                result?.Snapshot.Status.ToString() ?? "<none>",
                stopwatch.ElapsedMilliseconds);
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
            var stopwatch = Stopwatch.StartNew();
            var response = await _client.ValidateSourcesAsync(sourcePaths, timeout, cancellationToken).ConfigureAwait(false);
            var blockingMessage = response.HasBlockingMessage ? response.BlockingMessage : string.Empty;
            UiLogging.Information(
                "ValidateSourcesAsync completed. SourceCount={SourceCount} Blocking={Blocking} ElapsedMs={ElapsedMs}",
                sourcePaths.Count,
                blockingMessage.Length == 0 ? "<none>" : blockingMessage,
                stopwatch.ElapsedMilliseconds);
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
            var stopwatch = Stopwatch.StartNew();
            var response = await _client.WaitAsync(jobId, timeout, waitTimeout, cancellationToken).ConfigureAwait(false);
            if (response.Completed)
            {
                UiLogging.Information(
                    "WaitAsync completed. JobId={JobId} Completed=true WaitTimeoutMs={WaitTimeoutMs} ElapsedMs={ElapsedMs}",
                    jobId,
                    waitTimeout.TotalMilliseconds,
                    stopwatch.ElapsedMilliseconds);
            }
            else
            {
                UiLogging.Debug(
                    "WaitAsync completed. JobId={JobId} Completed=false WaitTimeoutMs={WaitTimeoutMs} ElapsedMs={ElapsedMs}",
                    jobId,
                    waitTimeout.TotalMilliseconds,
                    stopwatch.ElapsedMilliseconds);
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
        ImportJobStatusModel? lastLoggedStatus = null;
        string? lastLoggedMessage = null;
        int lastLoggedPercentBucket = -1;
        ulong lastLoggedProcessedBucket = ulong.MaxValue;
        var currentWaitTimeout = waitTimeout;

        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var completed = await WaitAsync(jobId, timeout, currentWaitTimeout, cancellationToken).ConfigureAwait(false);
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
                currentWaitTimeout = GetWaitTimeoutForStatus(snapshot.Status, waitTimeout);

                var statusChanged = snapshot.Status != lastLoggedStatus;
                var messageChanged = !string.IsNullOrEmpty(snapshot.Message) && snapshot.Message != lastLoggedMessage;
                var percentBucket = GetPercentLogBucket(snapshot.Percent);
                var processedBucket = GetProcessedEntriesLogBucket(snapshot.ProcessedEntries);
                var progressMilestoneChanged =
                    percentBucket != lastLoggedPercentBucket
                    || processedBucket != lastLoggedProcessedBucket;

                if (statusChanged || messageChanged || progressMilestoneChanged)
                {
                    UiLogging.Information(
                        "Import job in progress. JobId={JobId} Status={Status} Percent={Percent} " +
                        "Imported={Imported} Failed={Failed} Skipped={Skipped} Message={Message}",
                        jobId,
                        snapshot.Status,
                        snapshot.Percent,
                        snapshot.ImportedEntries,
                        snapshot.FailedEntries,
                        snapshot.SkippedEntries,
                        snapshot.Message.Length == 0 ? "<none>" : snapshot.Message);

                    lastLoggedStatus = snapshot.Status;
                    lastLoggedMessage = snapshot.Message;
                    lastLoggedPercentBucket = percentBucket;
                    lastLoggedProcessedBucket = processedBucket;
                }
            }
        }
    }

    internal static TimeSpan GetWaitTimeoutForStatus(ImportJobStatusModel status, TimeSpan defaultWaitTimeout) =>
        status switch
        {
            ImportJobStatusModel.RollingBack => RollingBackWaitTimeout,
            ImportJobStatusModel.Compacting => CompactingWaitTimeout,
            _ => defaultWaitTimeout
        };

    internal static int GetPercentLogBucket(double percent) =>
        Math.Clamp((int)percent, 0, 100) / PercentLogStep;

    internal static ulong GetProcessedEntriesLogBucket(ulong processedEntries) =>
        processedEntries / ProcessedEntriesLogStep;

    public async Task<bool> CancelAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            UiLogging.Warning("Cancelling import job. JobId={JobId} TimeoutMs={TimeoutMs}", jobId, timeout.TotalMilliseconds);
            var stopwatch = Stopwatch.StartNew();
            var response = await _client.CancelAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            var accepted = ImportJobMapper.FromProto(response);
            UiLogging.Information(
                "CancelAsync completed. JobId={JobId} Accepted={Accepted} ElapsedMs={ElapsedMs}",
                jobId,
                accepted,
                stopwatch.ElapsedMilliseconds);
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
            var stopwatch = Stopwatch.StartNew();
            var response = await _client.RemoveAsync(jobId, timeout, cancellationToken).ConfigureAwait(false);
            var removed = ImportJobMapper.FromProto(response);
            UiLogging.Information(
                "RemoveAsync completed. JobId={JobId} Removed={Removed} ElapsedMs={ElapsedMs}",
                jobId,
                removed,
                stopwatch.ElapsedMilliseconds);
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
            var stopwatch = Stopwatch.StartNew();
            var jobId = await _client.StartImportAsync(
                ImportJobMapper.ToProto(request),
                timeout,
                cancellationToken).ConfigureAwait(false);

            UiLogging.Information("Import job started. JobId={JobId} ElapsedMs={ElapsedMs}", jobId, stopwatch.ElapsedMilliseconds);
            return jobId;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to start import job. SourceCount={SourceCount}", request.SourcePaths.Count);
            throw;
        }
    }
}
