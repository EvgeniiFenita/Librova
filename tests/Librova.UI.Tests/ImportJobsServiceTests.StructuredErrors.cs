using Librova.UI.ImportJobs;
using System;
using System.Threading;
using System.Threading.Tasks;
using Xunit;

namespace Librova.UI.Tests;

public sealed partial class ImportJobsServiceTests
{
    [Fact]
    public async Task Service_ThrowsStructuredDomainExceptionForMissingCancelJob()
    {
        var service = new ImportJobsService(new StructuredErrorImportJobClient(
            cancelResponse: new Librova.V1.CancelImportJobResponse
            {
                Accepted = false,
                Error = new Librova.V1.DomainError
                {
                    Code = Librova.V1.ErrorCode.NotFound,
                    Message = "Import job 42 was not found for cancellation."
                }
            }));

        var error = await Assert.ThrowsAsync<ImportJobDomainException>(() =>
            service.CancelAsync(42, TimeSpan.FromSeconds(5), CancellationToken.None));
        Assert.Equal(ImportErrorCodeModel.NotFound, error.Error.Code);
    }

    [Fact]
    public async Task Service_ThrowsStructuredDomainExceptionForMissingRemoveJob()
    {
        var service = new ImportJobsService(new StructuredErrorImportJobClient(
            removeResponse: new Librova.V1.RemoveImportJobResponse
            {
                Removed = false,
                Error = new Librova.V1.DomainError
                {
                    Code = Librova.V1.ErrorCode.NotFound,
                    Message = "Import job 42 was not found for removal."
                }
            }));

        var error = await Assert.ThrowsAsync<ImportJobDomainException>(() =>
            service.RemoveAsync(42, TimeSpan.FromSeconds(5), CancellationToken.None));
        Assert.Equal(ImportErrorCodeModel.NotFound, error.Error.Code);
    }

    private sealed class StructuredErrorImportJobClient : ImportJobClient
    {
        private readonly Librova.V1.CancelImportJobResponse _cancelResponse;
        private readonly Librova.V1.RemoveImportJobResponse _removeResponse;

        public StructuredErrorImportJobClient(
            Librova.V1.CancelImportJobResponse? cancelResponse = null,
            Librova.V1.RemoveImportJobResponse? removeResponse = null)
            : base(@"\\.\pipe\Librova.UI.Tests.StructuredErrorImportJobs")
        {
            _cancelResponse = cancelResponse ?? new Librova.V1.CancelImportJobResponse { Accepted = true };
            _removeResponse = removeResponse ?? new Librova.V1.RemoveImportJobResponse { Removed = true };
        }

        public override Task<Librova.V1.CancelImportJobResponse> CancelAsync(
            ulong jobId,
            TimeSpan timeout,
            CancellationToken cancellationToken) => Task.FromResult(_cancelResponse);

        public override Task<Librova.V1.RemoveImportJobResponse> RemoveAsync(
            ulong jobId,
            TimeSpan timeout,
            CancellationToken cancellationToken) => Task.FromResult(_removeResponse);
    }
}

