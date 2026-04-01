using Librova.V1;
using Librova.UI.ImportJobs;
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
            AllowProbableDuplicates = true
        });

        Assert.Equal([@"C:\Books\book.fb2", @"C:\Books\folder"], proto.SourcePaths);
        Assert.Equal(@"C:\Work", proto.WorkingDirectory);
        Assert.Equal("abc123", proto.Sha256Hex);
        Assert.True(proto.AllowProbableDuplicates);
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
}
