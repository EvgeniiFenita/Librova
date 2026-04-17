using Librova.V1;
using Librova.UI.ImportJobs;
using System;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ImportJobMapperTests
{
    [Fact]
    public void Mapper_ConvertsStartRequestIntoProto()
    {
        var proto = ImportJobMapper.ToProto(new StartImportRequestModel
        {
            SourcePaths = [@"C:\Books\book.fb2", @"C:\Books\folder"],
            WorkingDirectory = @"C:\Work",
            Sha256Hex = "abc123",
            AllowProbableDuplicates = true,
            ForceEpubConversionOnImport = true
        });

        Assert.Equal([@"C:\Books\book.fb2", @"C:\Books\folder"], proto.SourcePaths);
        Assert.Equal(@"C:\Work", proto.WorkingDirectory);
        Assert.Equal("abc123", proto.Sha256Hex);
        Assert.True(proto.AllowProbableDuplicates);
        Assert.True(proto.ForceEpubConversion);
    }

    [Fact]
    public void Mapper_ConvertsResultIntoUiModels()
    {
        var model = ImportJobMapper.FromProto(new ImportJobResult
        {
            Snapshot = new ImportJobSnapshot
            {
                JobId = 5,
                Status = ImportJobStatus.Completed,
                Percent = 100,
                Message = "Done",
                TotalEntries = 4,
                ProcessedEntries = 4,
                ImportedEntries = 2,
                FailedEntries = 1,
                SkippedEntries = 1
            },
            Summary = new ImportSummary
            {
                Mode = ImportMode.ZipArchive,
                TotalEntries = 2,
                ImportedEntries = 1,
                FailedEntries = 1,
                SkippedEntries = 0
            },
            Error = new DomainError
            {
                Code = ErrorCode.DatabaseFailure,
                Message = "db"
            }
        });

        Assert.Equal(ImportJobStatusModel.Completed, model.Snapshot.Status);
        Assert.Equal(4UL, model.Snapshot.TotalEntries);
        Assert.Equal(4UL, model.Snapshot.ProcessedEntries);
        Assert.Equal(2UL, model.Snapshot.ImportedEntries);
        Assert.Equal(1UL, model.Snapshot.FailedEntries);
        Assert.Equal(1UL, model.Snapshot.SkippedEntries);
        Assert.Equal(ImportModeModel.ZipArchive, model.Summary!.Mode);
        Assert.Equal(ImportErrorCodeModel.DatabaseFailure, model.Error!.Code);
    }

    [Fact]
    public void Mapper_RejectsUnknownImportJobStatus()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            ImportJobMapper.FromProto(new ImportJobSnapshot
            {
                JobId = 5,
                Status = (ImportJobStatus)999,
                Message = "Drift"
            }));

        Assert.Contains("ImportJobStatus", error.Message, StringComparison.Ordinal);
        Assert.Contains("snapshot status", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownImportMode()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            ImportJobMapper.FromProto(new ImportJobResult
            {
                Snapshot = new ImportJobSnapshot
                {
                    JobId = 5,
                    Status = ImportJobStatus.Completed
                },
                Summary = new ImportSummary
                {
                    Mode = (ImportMode)999
                }
            }));

        Assert.Contains("ImportMode", error.Message, StringComparison.Ordinal);
        Assert.Contains("summary mode", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_ThrowsStructuredDomainExceptionForCancelError()
    {
        var error = Assert.Throws<ImportJobDomainException>(() =>
            ImportJobMapper.FromProto(new CancelImportJobResponse
            {
                Accepted = false,
                Error = new DomainError
                {
                    Code = ErrorCode.NotFound,
                    Message = "Import job 42 was not found for cancellation."
                }
            }));

        Assert.Equal(ImportErrorCodeModel.NotFound, error.Error.Code);
        Assert.Equal("Import job 42 was not found for cancellation.", error.Message);
    }

    [Fact]
    public void Mapper_ThrowsStructuredDomainExceptionForRemoveError()
    {
        var error = Assert.Throws<ImportJobDomainException>(() =>
            ImportJobMapper.FromProto(new RemoveImportJobResponse
            {
                Removed = false,
                Error = new DomainError
                {
                    Code = ErrorCode.NotFound,
                    Message = "Import job 42 was not found for removal."
                }
            }));

        Assert.Equal(ImportErrorCodeModel.NotFound, error.Error.Code);
        Assert.Equal("Import job 42 was not found for removal.", error.Message);
    }

    [Fact]
    public void Mapper_RejectsUnknownDomainErrorCodeInResult()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            ImportJobMapper.FromProto(new ImportJobResult
            {
                Snapshot = new ImportJobSnapshot
                {
                    JobId = 5,
                    Status = ImportJobStatus.Completed
                },
                Error = new DomainError
                {
                    Code = (ErrorCode)999,
                    Message = "drift"
                }
            }));

        Assert.Contains("ErrorCode", error.Message, StringComparison.Ordinal);
        Assert.Contains("domain error code", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownDomainErrorCodeInCancelResponse()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            ImportJobMapper.FromProto(new CancelImportJobResponse
            {
                Error = new DomainError
                {
                    Code = (ErrorCode)999,
                    Message = "drift"
                }
            }));

        Assert.Contains("ErrorCode", error.Message, StringComparison.Ordinal);
        Assert.Contains("domain error code", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownDomainErrorCodeInRemoveResponse()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            ImportJobMapper.FromProto(new RemoveImportJobResponse
            {
                Error = new DomainError
                {
                    Code = (ErrorCode)999,
                    Message = "drift"
                }
            }));

        Assert.Contains("ErrorCode", error.Message, StringComparison.Ordinal);
        Assert.Contains("domain error code", error.Message, StringComparison.Ordinal);
    }
}

