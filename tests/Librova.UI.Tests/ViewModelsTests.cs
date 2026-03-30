using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.ViewModels;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ViewModelsTests
{
    [Fact]
    public async Task ImportJobsViewModel_StartsImportAndPollsUntilTerminalResult()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.NotNull(viewModel.LastResult);
            Assert.Equal(ImportJobStatusModel.Completed, viewModel.LastResult!.Snapshot.Status);
            Assert.Equal("Completed", viewModel.StatusText);
            Assert.Equal("Imported 1 of 1; failed 0; skipped 0.", viewModel.ResultSummaryText);
            Assert.Equal("Watch for duplicates", viewModel.WarningsText);
            Assert.Equal("No error.", viewModel.ErrorText);
            Assert.True(service.TryGetSnapshotCalls > 0);
            Assert.False(viewModel.IsBusy);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public void ImportJobsViewModel_CommandRequiresPaths()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService());

        Assert.False(viewModel.StartImportCommand.CanExecute(null));

        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            viewModel.SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2");
            viewModel.WorkingDirectory = Path.Combine(sandboxRoot, "work");

            Assert.True(viewModel.StartImportCommand.CanExecute(null));
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task ImportJobsViewModel_SetsControlledErrorStateWhenImportFails()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var viewModel = new ImportJobsViewModel(new FailingImportJobsService())
        {
            SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
            WorkingDirectory = Path.Combine(sandboxRoot, "work")
        };

        try
        {
            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.Equal("transport failed", viewModel.StatusText);
            Assert.False(viewModel.IsBusy);
            Assert.Null(viewModel.LastResult);
            Assert.Equal("No completed job yet.", viewModel.ResultSummaryText);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task ImportJobsViewModel_RefreshCommandLoadsCurrentSnapshot()
    {
        var service = new SnapshotOnlyImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportAsync();
            service.ReturnTerminalResult = false;
            await viewModel.RefreshCommand.ExecuteAsyncForTests();

            Assert.Equal("Still running", viewModel.StatusText);
            Assert.True(service.RefreshSnapshotCalls > 0);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task ImportJobsViewModel_CancelCommandRequestsCancellationForActiveJob()
    {
        var service = new CancelAwareImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var runTask = viewModel.StartImportAsync();
            await service.WaitForStartAsync();
            await service.WaitForPollingAsync();

            Assert.True(viewModel.CancelImportCommand.CanExecute(null));

            await viewModel.CancelImportCommand.ExecuteAsyncForTests();
            Assert.Contains("cancellation requested", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
            await runTask;

            Assert.True(service.CancelCalls > 0);
            Assert.Equal(ImportJobStatusModel.Cancelled, viewModel.LastResult?.Snapshot.Status);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task ImportJobsViewModel_RemoveCommandClearsCurrentJobState()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportAsync();
            Assert.True(viewModel.RemoveJobCommand.CanExecute(null));

            await viewModel.RemoveJobCommand.ExecuteAsyncForTests();

            Assert.Equal(42UL, service.LastRemovedJobId);
            Assert.Null(viewModel.LastJobId);
            Assert.Null(viewModel.LastResult);
            Assert.Equal("No completed job yet.", viewModel.ResultSummaryText);
            Assert.Contains("was removed", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task ImportJobsViewModel_BrowseCommandsPopulatePathsFromSelectionService()
    {
        var selectionService = new FakePathSelectionService
        {
            SelectedSourcePath = @"C:\Incoming\book.fb2",
            SelectedWorkingDirectory = @"C:\Temp\Librova\Work"
        };
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService(), selectionService);

        await viewModel.BrowseSourceCommand.ExecuteAsyncForTests();
        await viewModel.BrowseWorkingDirectoryCommand.ExecuteAsyncForTests();

        Assert.Equal(@"C:\Incoming\book.fb2", viewModel.SourcePath);
        Assert.Equal(@"C:\Temp\Librova\Work", viewModel.WorkingDirectory);
    }

    [Fact]
    public async Task ImportJobsViewModel_StartImportPassesProbableDuplicateOverrideFlag()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work"),
                AllowProbableDuplicates = true
            };

            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.NotNull(service.LastStartRequest);
            Assert.True(service.LastStartRequest!.AllowProbableDuplicates);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public void ImportJobsViewModel_DisablesStartForMissingSourceFile()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
        {
            SourcePath = Path.Combine(Path.GetTempPath(), $"{Guid.NewGuid():N}.fb2"),
            WorkingDirectory = @"C:\Work"
        };

        Assert.False(viewModel.StartImportCommand.CanExecute(null));
        Assert.Equal("Source file does not exist.", viewModel.SourceValidationMessage);
    }

    [Fact]
    public async Task ImportJobsViewModel_EnablesStartForExistingSupportedSourceFile()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath, "<fb2 />");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            Assert.True(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal(string.Empty, viewModel.SourceValidationMessage);
            Assert.Equal(string.Empty, viewModel.WorkingDirectoryValidationMessage);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public void ImportJobsViewModel_DisablesStartForUnsupportedSourceExtension()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.txt");
            File.WriteAllText(sourcePath, "plain text");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            Assert.False(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal("Supported source types are .fb2, .epub, and .zip.", viewModel.SourceValidationMessage);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public void ImportJobsViewModel_DisablesStartWhenWorkingDirectoryPointsToFile()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.epub");
            File.WriteAllText(sourcePath, "epub");
            var filePath = Path.Combine(sandboxRoot, "not-a-directory.tmp");
            File.WriteAllText(filePath, "tmp");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = filePath
            };

            Assert.False(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal("Working directory must not point to a file.", viewModel.WorkingDirectoryValidationMessage);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public void ImportJobsViewModel_ApplyDroppedSourcePathUpdatesSourceField()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService());

        viewModel.ApplyDroppedSourcePath(@"C:\Incoming\dropped.epub");

        Assert.Equal(@"C:\Incoming\dropped.epub", viewModel.SourcePath);
    }

    [Fact]
    public void ImportJobsViewModel_ApplyDroppedSourcePathIgnoresBlankValues()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
        {
            SourcePath = @"C:\Incoming\existing.fb2"
        };

        viewModel.ApplyDroppedSourcePath("   ");

        Assert.Equal(@"C:\Incoming\existing.fb2", viewModel.SourcePath);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LoadsBooksFromCatalogService()
    {
        var viewModel = new LibraryBrowserViewModel(new FakeLibraryCatalogService());
        viewModel.SearchText = "road";

        await viewModel.RefreshAsync();

        Assert.True(viewModel.HasBooks);
        Assert.Equal("Loaded 1 book(s) for page 1.", viewModel.StatusText);
        Assert.Single(viewModel.Books);
        Assert.Equal("Roadside Picnic", viewModel.Books[0].Title);
        Assert.NotNull(viewModel.SelectedBook);
        Assert.Contains("Arkady Strugatsky", viewModel.SelectedBookDetailsText, StringComparison.Ordinal);
        Assert.Contains("Load Full Details", viewModel.SelectedBookExtendedDetailsText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ReportsEmptySearchResult()
    {
        var viewModel = new LibraryBrowserViewModel(new EmptyLibraryCatalogService());

        await viewModel.RefreshCommand.ExecuteAsyncForTests();

        Assert.False(viewModel.HasBooks);
        Assert.Equal("No books found for the current filter.", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_PassesFiltersAndSortIntoCatalogRequest()
    {
        var service = new RecordingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            SearchText = "road",
            AuthorFilter = "Arkady",
            LanguageFilter = "en",
            SelectedFormatFilter = "FB2",
            SelectedSort = BookSortModel.Author,
            PageSize = 10
        };

        await viewModel.RefreshAsync();

        Assert.NotNull(service.LastRequest);
        Assert.Equal("road", service.LastRequest!.Text);
        Assert.Equal("Arkady", service.LastRequest.Author);
        Assert.Equal("en", service.LastRequest.Language);
        Assert.Equal(BookFormatModel.Fb2, service.LastRequest.Format);
        Assert.Equal(BookSortModel.Author, service.LastRequest.SortBy);
        Assert.Equal(0UL, service.LastRequest.Offset);
        Assert.Equal(11UL, service.LastRequest.Limit);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_PaginatesForwardAndBackward()
    {
        var service = new PagingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();
        Assert.True(viewModel.HasMoreResults);
        Assert.Equal("Page 1+", viewModel.PageLabelText);
        Assert.Equal("Alpha", viewModel.SelectedBookTitle);

        await viewModel.NextPageAsync();
        Assert.Equal(2, viewModel.CurrentPage);
        Assert.Equal("Page 2", viewModel.PageLabelText);
        Assert.Equal("Gamma", viewModel.SelectedBookTitle);

        await viewModel.PreviousPageAsync();
        Assert.Equal(1, viewModel.CurrentPage);
        Assert.Equal("Alpha", viewModel.SelectedBookTitle);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_DoesNotExposePhantomNextPageForExactlyFullPage()
    {
        var service = new ExactPageLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();

        Assert.False(viewModel.HasMoreResults);
        Assert.Equal("Page 1", viewModel.PageLabelText);
        Assert.False(viewModel.NextPageCommand.CanExecute(null));
        Assert.Equal(3UL, service.LastRequest!.Limit);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LoadsExtendedDetailsForSelectedBook()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        Assert.NotNull(viewModel.SelectedBook);

        await viewModel.LoadDetailsCommand.ExecuteAsyncForTests();

        Assert.NotNull(viewModel.SelectedBookDetails);
        Assert.Contains("Macmillan", viewModel.SelectedBookExtendedDetailsText, StringComparison.Ordinal);
        Assert.Contains("Aliens land only in one city.", viewModel.SelectedBookExtendedDetailsText, StringComparison.Ordinal);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ExportsSelectedBookThroughPathSelectionService()
    {
        var service = new FakeLibraryCatalogService();
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\alpha.epub"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService);

        await viewModel.RefreshAsync();
        Assert.NotNull(viewModel.SelectedBook);

        await viewModel.ExportSelectedBookAsync();

        Assert.Equal(viewModel.SelectedBook!.BookId, service.LastExportBookId);
        Assert.Equal(@"C:\Exports\alpha.epub", service.LastExportPath);
        Assert.Contains("Exported", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_MovesSelectedBookToTrashThroughCatalogService()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        Assert.NotNull(viewModel.SelectedBook);
        var selectedBookId = viewModel.SelectedBook!.BookId;

        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.Equal(selectedBookId, service.LastTrashedBookId);
        Assert.Contains("Moved", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void LibraryBrowserViewModel_ClampsInvalidPageSize()
    {
        var viewModel = new LibraryBrowserViewModel(new EmptyLibraryCatalogService())
        {
            PageSize = 0
        };

        Assert.Equal(1, viewModel.PageSize);

        viewModel.PageSize = -10;

        Assert.Equal(1, viewModel.PageSize);
        Assert.Equal("1 per page", viewModel.PageSizeText);
    }

    [Fact]
    public async Task ImportJobsViewModel_EmitsSuccessEventForCompletedImport()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var invoked = 0;
            viewModel.ImportCompletedSuccessfully += () =>
            {
                invoked++;
                return Task.CompletedTask;
            };

            await viewModel.StartImportAsync();

            Assert.Equal(1, invoked);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    private sealed class FakeImportJobsService : IImportJobsService
    {
        public int TryGetSnapshotCalls { get; private set; }
        public ulong? LastRemovedJobId { get; private set; }
        public StartImportRequestModel? LastStartRequest { get; private set; }
        private bool _resultReady;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(_resultReady
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Completed,
                        Message = "Completed"
                    },
                    Summary = new ImportSummaryModel
                    {
                        Mode = ImportModeModel.SingleFile,
                        TotalEntries = 1,
                        ImportedEntries = 1,
                        FailedEntries = 0,
                        SkippedEntries = 0,
                        Warnings = ["Watch for duplicates"]
                    }
                }
                : null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            TryGetSnapshotCalls++;
            _resultReady = true;
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running"
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRemovedJobId = jobId;
            return Task.FromResult(true);
        }

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastStartRequest = request;
            return Task.FromResult(42UL);
        }

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class FailingImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(null);

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => throw new InvalidOperationException("transport failed");

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class SnapshotOnlyImportJobsService : IImportJobsService
    {
        public int RefreshSnapshotCalls { get; private set; }
        public bool ReturnTerminalResult { get; set; } = true;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(ReturnTerminalResult
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Completed,
                        Message = "Completed"
                    }
                }
                : null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            RefreshSnapshotCalls++;
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Still running"
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(64UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(false);
    }

    private sealed class CancelAwareImportJobsService : IImportJobsService
    {
        private readonly TaskCompletionSource _started = new();
        private readonly TaskCompletionSource _polling = new();
        private volatile bool _cancelled;

        public int CancelCalls { get; private set; }
        public Task WaitForStartAsync() => _started.Task;
        public Task WaitForPollingAsync() => _polling.Task;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            CancelCalls++;
            _cancelled = true;
            return Task.FromResult(true);
        }

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _started.TrySetResult();

            if (_cancelled)
            {
                return Task.FromResult<ImportJobResultModel?>(new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Cancelled,
                        Message = "Cancelled"
                    }
                });
            }

            return Task.FromResult<ImportJobResultModel?>(null);
        }

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _polling.TrySetResult();
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running"
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(77UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public string? SelectedSourcePath { get; init; }
        public string? SelectedWorkingDirectory { get; init; }
        public string? SelectedExportPath { get; init; }

        public Task<string?> PickSourceFileAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourcePath);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

        public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
            => Task.FromResult(SelectedExportPath);
    }

    private static string CreateSupportedSourceFile(string sandboxRoot, string fileName)
    {
        var sourcePath = Path.Combine(sandboxRoot, fileName);
        File.WriteAllText(sourcePath, "content");
        return sourcePath;
    }

    private sealed class FakeLibraryCatalogService : ILibraryCatalogService
    {
        public long? LastExportBookId { get; private set; }
        public string? LastExportPath { get; private set; }
        public long? LastTrashedBookId { get; private set; }

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<IReadOnlyList<BookListItemModel>>([
                new BookListItemModel
                {
                    BookId = 1,
                    Title = "Roadside Picnic",
                    Authors = ["Arkady Strugatsky"],
                    Language = "en",
                    Format = BookFormatModel.Epub,
                    ManagedPath = "Books/0000000001/book.epub",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Roadside Picnic",
                Authors = ["Arkady Strugatsky"],
                Language = "en",
                Publisher = "Macmillan",
                Year = 1972,
                Isbn = "978-5-17-000000-1",
                Tags = ["sci-fi"],
                Description = "Aliens land only in one city.",
                Identifier = "details-id",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/book.epub",
                SizeBytes = 4096,
                Sha256Hex = "details-hash",
                AddedAtUtc = DateTimeOffset.UtcNow
            });

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastExportBookId = bookId;
            LastExportPath = destinationPath;
            return Task.FromResult<string?>(destinationPath);
        }

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastTrashedBookId = bookId;
            return Task.FromResult(true);
        }
    }

    private sealed class EmptyLibraryCatalogService : ILibraryCatalogService
    {
        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<IReadOnlyList<BookListItemModel>>([]);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(false);
    }

    private sealed class RecordingLibraryCatalogService : ILibraryCatalogService
    {
        public BookListRequestModel? LastRequest { get; private set; }
        public (long BookId, string DestinationPath)? LastExportRequest { get; private set; }

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRequest = request;
            return Task.FromResult<IReadOnlyList<BookListItemModel>>([]);
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastExportRequest = (bookId, destinationPath);
            return Task.FromResult<string?>(destinationPath);
        }

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class PagingLibraryCatalogService : ILibraryCatalogService
    {
        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            if (request.Offset == 0)
            {
                return Task.FromResult<IReadOnlyList<BookListItemModel>>([
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Alpha",
                        Authors = ["Author One"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/alpha.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    },
                    new BookListItemModel
                    {
                        BookId = 2,
                        Title = "Beta",
                        Authors = ["Author Two"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000002/beta.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    },
                    new BookListItemModel
                    {
                        BookId = 99,
                        Title = "Lookahead",
                        Authors = ["Author Extra"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000099/lookahead.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]);
            }

            return Task.FromResult<IReadOnlyList<BookListItemModel>>([
                new BookListItemModel
                {
                    BookId = 3,
                    Title = "Gamma",
                    Authors = ["Author Three"],
                    Language = "en",
                    Format = BookFormatModel.Fb2,
                    ManagedPath = "Books/0000000003/gamma.fb2",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]);
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class ExactPageLibraryCatalogService : ILibraryCatalogService
    {
        public BookListRequestModel? LastRequest { get; private set; }

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRequest = request;
            return Task.FromResult<IReadOnlyList<BookListItemModel>>([
                new BookListItemModel
                {
                    BookId = 1,
                    Title = "Alpha",
                    Authors = ["Author One"],
                    Language = "en",
                    Format = BookFormatModel.Epub,
                    ManagedPath = "Books/0000000001/alpha.epub",
                    AddedAtUtc = DateTimeOffset.UtcNow
                },
                new BookListItemModel
                {
                    BookId = 2,
                    Title = "Beta",
                    Authors = ["Author Two"],
                    Language = "en",
                    Format = BookFormatModel.Fb2,
                    ManagedPath = "Books/0000000002/beta.fb2",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]);
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }
}
