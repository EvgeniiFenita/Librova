using System;
using System.Collections.Generic;

namespace Librova.UI.ImportJobs;

internal enum ImportJobStatusModel
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
}

internal enum ImportModeModel
{
    SingleFile,
    ZipArchive,
    Batch
}

internal enum ImportErrorCodeModel
{
    Validation,
    UnsupportedFormat,
    DuplicateRejected,
    DuplicateDecisionRequired,
    ParserFailure,
    ConverterUnavailable,
    ConverterFailed,
    StorageFailure,
    DatabaseFailure,
    Cancellation,
    IntegrityIssue,
    Unknown
}

internal sealed class StartImportRequestModel
{
    public required IReadOnlyList<string> SourcePaths { get; init; }
    public required string WorkingDirectory { get; init; }
    public string? Sha256Hex { get; init; }
    public bool AllowProbableDuplicates { get; init; }
}

internal sealed class ImportSummaryModel
{
    public ImportModeModel Mode { get; init; }
    public ulong TotalEntries { get; init; }
    public ulong ImportedEntries { get; init; }
    public ulong FailedEntries { get; init; }
    public ulong SkippedEntries { get; init; }
    public IReadOnlyList<string> Warnings { get; init; } = [];
}

internal sealed class ImportJobSnapshotModel
{
    public ulong JobId { get; init; }
    public ImportJobStatusModel Status { get; init; }
    public int Percent { get; init; }
    public string Message { get; init; } = string.Empty;
    public IReadOnlyList<string> Warnings { get; init; } = [];
}

internal sealed class DomainErrorModel
{
    public ImportErrorCodeModel Code { get; init; }
    public string Message { get; init; } = string.Empty;
}

internal sealed class ImportJobResultModel
{
    public required ImportJobSnapshotModel Snapshot { get; init; }
    public ImportSummaryModel? Summary { get; init; }
    public DomainErrorModel? Error { get; init; }
}
