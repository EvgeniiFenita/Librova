using Librova.V1;
using System;
using System.Linq;

namespace Librova.UI.ImportJobs;

internal static class ImportJobMapper
{
    private static InvalidOperationException CreateUnexpectedEnumValueException<TEnum>(TEnum value, string context)
        where TEnum : struct, Enum =>
        new($"Received unexpected {typeof(TEnum).Name} value '{value}' while mapping {context}.");

    private static ImportErrorCodeModel MapErrorCode(ErrorCode code) =>
        code switch
        {
            ErrorCode.Validation => ImportErrorCodeModel.Validation,
            ErrorCode.UnsupportedFormat => ImportErrorCodeModel.UnsupportedFormat,
            ErrorCode.DuplicateRejected => ImportErrorCodeModel.DuplicateRejected,
            ErrorCode.ParserFailure => ImportErrorCodeModel.ParserFailure,
            ErrorCode.ConverterUnavailable => ImportErrorCodeModel.ConverterUnavailable,
            ErrorCode.ConverterFailed => ImportErrorCodeModel.ConverterFailed,
            ErrorCode.StorageFailure => ImportErrorCodeModel.StorageFailure,
            ErrorCode.DatabaseFailure => ImportErrorCodeModel.DatabaseFailure,
            ErrorCode.Cancellation => ImportErrorCodeModel.Cancellation,
            ErrorCode.IntegrityIssue => ImportErrorCodeModel.IntegrityIssue,
            ErrorCode.NotFound => ImportErrorCodeModel.NotFound,
            _ => throw CreateUnexpectedEnumValueException(code, "domain error code")
        };

    private static DomainErrorModel MapDomainError(DomainError error) =>
        new()
        {
            Code = MapErrorCode(error.Code),
            Message = error.Message
        };

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

    public static bool FromProto(CancelImportJobResponse response) =>
        response.Error is null
            ? response.Accepted
            : throw new ImportJobDomainException(MapDomainError(response.Error));

    public static bool FromProto(RemoveImportJobResponse response) =>
        response.Error is null
            ? response.Removed
            : throw new ImportJobDomainException(MapDomainError(response.Error));

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
                _ => throw CreateUnexpectedEnumValueException(snapshot.Status, "import job snapshot status")
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
                    _ => throw CreateUnexpectedEnumValueException(result.Summary.Mode, "import summary mode")
                },
                TotalEntries = result.Summary.TotalEntries,
                ImportedEntries = result.Summary.ImportedEntries,
                FailedEntries = result.Summary.FailedEntries,
                SkippedEntries = result.Summary.SkippedEntries,
                Warnings = result.Summary.Warnings.ToArray()
            },
            Error = result.Error is null ? null : MapDomainError(result.Error)
        };
}
