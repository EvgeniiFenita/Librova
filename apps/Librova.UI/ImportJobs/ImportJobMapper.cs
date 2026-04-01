using Librova.V1;
using System.Linq;

namespace Librova.UI.ImportJobs;

internal static class ImportJobMapper
{
    public static ImportRequest ToProto(StartImportRequestModel model)
    {
        var request = new ImportRequest
        {
            WorkingDirectory = model.WorkingDirectory,
            AllowProbableDuplicates = model.AllowProbableDuplicates,
            ForceEpubConversion = model.ForceEpubConversionOnImport
        };
        request.SourcePaths.AddRange(model.SourcePaths);

        if (!string.IsNullOrWhiteSpace(model.Sha256Hex))
        {
            request.Sha256Hex = model.Sha256Hex;
        }

        return request;
    }

    public static ImportJobSnapshotModel? FromProto(GetImportJobSnapshotResponse response)
        => response.Snapshot is null ? null : FromProto(response.Snapshot);

    public static ImportJobResultModel? FromProto(GetImportJobResultResponse response)
        => response.Result is null ? null : FromProto(response.Result);

    public static ImportJobSnapshotModel FromProto(ImportJobSnapshot snapshot) =>
        new()
        {
            JobId = snapshot.JobId,
            Status = snapshot.Status switch
            {
                ImportJobStatus.Pending => ImportJobStatusModel.Pending,
                ImportJobStatus.Running => ImportJobStatusModel.Running,
                ImportJobStatus.Completed => ImportJobStatusModel.Completed,
                ImportJobStatus.Failed => ImportJobStatusModel.Failed,
                ImportJobStatus.Cancelled => ImportJobStatusModel.Cancelled,
                _ => ImportJobStatusModel.Failed
            },
            Percent = snapshot.Percent,
            Message = snapshot.Message,
            Warnings = snapshot.Warnings.ToArray(),
            TotalEntries = snapshot.TotalEntries,
            ProcessedEntries = snapshot.ProcessedEntries,
            ImportedEntries = snapshot.ImportedEntries,
            FailedEntries = snapshot.FailedEntries,
            SkippedEntries = snapshot.SkippedEntries
        };

    public static ImportJobResultModel FromProto(ImportJobResult result) =>
        new()
        {
            Snapshot = FromProto(result.Snapshot),
            Summary = result.Summary is null ? null : new ImportSummaryModel
            {
                Mode = result.Summary.Mode switch
                {
                    ImportMode.SingleFile => ImportModeModel.SingleFile,
                    ImportMode.ZipArchive => ImportModeModel.ZipArchive,
                    ImportMode.Batch => ImportModeModel.Batch,
                    _ => ImportModeModel.SingleFile
                },
                TotalEntries = result.Summary.TotalEntries,
                ImportedEntries = result.Summary.ImportedEntries,
                FailedEntries = result.Summary.FailedEntries,
                SkippedEntries = result.Summary.SkippedEntries,
                Warnings = result.Summary.Warnings.ToArray()
            },
            Error = result.Error is null ? null : new DomainErrorModel
            {
                Code = result.Error.Code switch
                {
                    ErrorCode.Validation => ImportErrorCodeModel.Validation,
                    ErrorCode.UnsupportedFormat => ImportErrorCodeModel.UnsupportedFormat,
                    ErrorCode.DuplicateRejected => ImportErrorCodeModel.DuplicateRejected,
                    ErrorCode.DuplicateDecisionRequired => ImportErrorCodeModel.DuplicateDecisionRequired,
                    ErrorCode.ParserFailure => ImportErrorCodeModel.ParserFailure,
                    ErrorCode.ConverterUnavailable => ImportErrorCodeModel.ConverterUnavailable,
                    ErrorCode.ConverterFailed => ImportErrorCodeModel.ConverterFailed,
                    ErrorCode.StorageFailure => ImportErrorCodeModel.StorageFailure,
                    ErrorCode.DatabaseFailure => ImportErrorCodeModel.DatabaseFailure,
                    ErrorCode.Cancellation => ImportErrorCodeModel.Cancellation,
                    ErrorCode.IntegrityIssue => ImportErrorCodeModel.IntegrityIssue,
                    _ => ImportErrorCodeModel.Unknown
                },
                Message = result.Error.Message
            }
        };
}
