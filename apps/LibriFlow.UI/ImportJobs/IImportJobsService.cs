using System;
using System.Threading;
using System.Threading.Tasks;

namespace LibriFlow.UI.ImportJobs;

internal interface IImportJobsService
{
    Task<ulong> StartAsync(
        StartImportRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<ImportJobResultModel?> TryGetResultAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<bool> WaitAsync(
        ulong jobId,
        TimeSpan timeout,
        TimeSpan waitTimeout,
        CancellationToken cancellationToken);

    Task<bool> CancelAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<bool> RemoveAsync(
        ulong jobId,
        TimeSpan timeout,
        CancellationToken cancellationToken);
}
