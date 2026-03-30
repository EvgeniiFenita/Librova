using Libriflow.V1;
using LibriFlow.UI.ImportJobs;
using Xunit;

namespace LibriFlow.UI.Tests;

public sealed class ImportJobMapperTests
{
    [Fact]
    public void Mapper_ConvertsStartRequestIntoProto()
    {
        var proto = ImportJobMapper.ToProto(new StartImportRequestModel
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work",
            Sha256Hex = "abc123",
            AllowProbableDuplicates = true
        });

        Assert.Equal(@"C:\Books\book.fb2", proto.SourcePath);
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
                Message = "Done"
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
        Assert.Equal(ImportModeModel.ZipArchive, model.Summary!.Mode);
        Assert.Equal(ImportErrorCodeModel.DatabaseFailure, model.Error!.Code);
    }
}
