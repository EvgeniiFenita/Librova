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
            SelectedSourcePaths = [@"C:\Incoming\book.fb2", @"C:\Incoming\batch.epub"],
            SelectedWorkingDirectory = @"C:\Temp\Librova\Work"
        };
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService(), selectionService);

        await viewModel.BrowseSourceCommand.ExecuteAsyncForTests();
        await viewModel.BrowseWorkingDirectoryCommand.ExecuteAsyncForTests();

        Assert.Equal([@"C:\Incoming\book.fb2", @"C:\Incoming\batch.epub"], viewModel.SourcePaths);
        Assert.Equal(@"C:\Temp\Librova\Work", viewModel.WorkingDirectory);
    }

    [Fact]
    public async Task ImportJobsViewModel_SelectSourceAndImportCommand_StartsImportAfterPickingSource()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = CreateSupportedSourceFile(sandboxRoot, "selected.fb2");
            var viewModel = new ImportJobsViewModel(
                service,
                new FakePathSelectionService
                {
                    SelectedSourcePaths = [sourcePath]
                })
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.SelectSourceAndImportCommand.ExecuteAsyncForTests();

            Assert.Equal([sourcePath], viewModel.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.NotNull(viewModel.LastResult);
            Assert.Equal([sourcePath], service.LastStartRequest?.SourcePaths);
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
    public async Task ImportJobsViewModel_SelectSourceAndImportCommand_StartsBatchImportAfterPickingMultipleFiles()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var firstSourcePath = CreateSupportedSourceFile(sandboxRoot, "selected-one.fb2");
            var secondSourcePath = CreateSupportedSourceFile(sandboxRoot, "selected-two.epub");
            var viewModel = new ImportJobsViewModel(
                service,
                new FakePathSelectionService
                {
                    SelectedSourcePaths = [firstSourcePath, secondSourcePath]
                })
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.SelectSourceAndImportCommand.ExecuteAsyncForTests();

            Assert.Equal([firstSourcePath, secondSourcePath], viewModel.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.Equal([firstSourcePath, secondSourcePath], service.LastStartRequest?.SourcePaths);
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
    public async Task ImportJobsViewModel_SelectDirectoryAndImportCommand_StartsRecursiveDirectoryImport()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourceDirectory = Path.Combine(sandboxRoot, "incoming");
            Directory.CreateDirectory(sourceDirectory);
            var viewModel = new ImportJobsViewModel(
                service,
                new FakePathSelectionService
                {
                    SelectedSourceDirectory = sourceDirectory
                })
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.SelectDirectoryAndImportCommand.ExecuteAsyncForTests();

            Assert.Equal([sourceDirectory], viewModel.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.Equal([sourceDirectory], service.LastStartRequest?.SourcePaths);
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
    public async Task ImportJobsViewModel_ApplyDroppedSourcePathsAndStartAsync_HandlesStartFailuresWithoutThrowing()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = CreateSupportedSourceFile(sandboxRoot, "dropped.fb2");
            var viewModel = new ImportJobsViewModel(new FailingImportJobsService())
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.ApplyDroppedSourcePathsAndStartAsync([sourcePath]);

            Assert.Equal([sourcePath], viewModel.SourcePaths);
            Assert.Equal("transport failed", viewModel.StatusText);
            Assert.False(viewModel.IsBusy);
            Assert.Null(viewModel.LastResult);
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
    public async Task ImportJobsViewModel_AllowsMixedValidAndUnsupportedSelectedSourcesWhenAtLeastOneSourceIsImportable()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var validSourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2");
            var unsupportedSourcePath = Path.Combine(sandboxRoot, "notes.txt");
            await File.WriteAllTextAsync(unsupportedSourcePath, "ignored");

            var viewModel = new ImportJobsViewModel(service)
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            viewModel.ApplyDroppedSourcePaths([validSourcePath, unsupportedSourcePath]);

            Assert.True(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal(string.Empty, viewModel.SourceValidationMessage);

            await viewModel.StartImportAsync();

            Assert.Equal([validSourcePath, unsupportedSourcePath], service.LastStartRequest?.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
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
        Assert.Equal("A selected source does not exist.", viewModel.SourceValidationMessage);
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
            Assert.Equal("Supported source types are .fb2, .epub, and .zip, or a directory containing them.", viewModel.SourceValidationMessage);
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
        Assert.Equal("Loaded 1 book(s).", viewModel.StatusText);
        Assert.Single(viewModel.Books);
        Assert.Equal("Roadside Picnic", viewModel.Books[0].Title);
        Assert.Null(viewModel.SelectedBook);
        Assert.Equal("1 shown", viewModel.BookCountText);
        Assert.True(viewModel.Books[0].ShowCoverPlaceholder);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LoadsCoverImageThroughLoaderAgainstLibraryRoot()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        var coversRoot = Path.Combine(sandboxRoot, "Covers");
        Directory.CreateDirectory(coversRoot);

        try
        {
            var coverPath = Path.Combine(coversRoot, "0000000001.png");
            await File.WriteAllTextAsync(coverPath, "cover-path-marker");

            var service = new CoverAwareLibraryCatalogService();
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Single(loader.LoadedPaths);
            Assert.Equal(coverPath, loader.LoadedPaths[0]);

            await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

            Assert.Equal(2, loader.LoadedPaths.Count);
            Assert.Equal(coverPath, loader.LoadedPaths[1]);
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
    public async Task LibraryBrowserViewModel_ReportsEmptySearchResult()
    {
        var viewModel = new LibraryBrowserViewModel(new EmptyLibraryCatalogService());

        await viewModel.RefreshCommand.ExecuteAsyncForTests();

        Assert.False(viewModel.HasBooks);
        Assert.Equal("No books found for the current filter.", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_PassesSearchAndLanguageIntoCatalogRequest()
    {
        var service = new RecordingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            SearchText = "road",
            LanguageFilter = "en",
            PageSize = 10
        };

        await viewModel.RefreshAsync();

        Assert.NotNull(service.LastRequest);
        Assert.Equal("road", service.LastRequest!.Text);
        Assert.Equal("en", service.LastRequest.Language);
        Assert.Null(service.LastRequest.Author);
        Assert.Null(service.LastRequest.Format);
        Assert.Null(service.LastRequest.SortBy);
        Assert.Equal(0UL, service.LastRequest.Offset);
        Assert.Equal(11UL, service.LastRequest.Limit);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_SearchRefreshesAsTextChangesAndRestoresBooksAfterClearing()
    {
        var service = new QueryFilteringLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        Assert.Equal(2, viewModel.Books.Count);

        viewModel.SearchText = "road";
        await WaitForConditionAsync(() => service.ListCalls >= 2 && viewModel.Books.Count == 1);

        Assert.Single(viewModel.Books);
        Assert.Equal("Roadside Picnic", viewModel.Books[0].Title);
        Assert.Equal("1 shown", viewModel.BookCountText);

        viewModel.SearchText = "missing";
        await WaitForConditionAsync(() => service.ListCalls >= 3 && viewModel.Books.Count == 0);

        Assert.False(viewModel.HasBooks);
        Assert.Equal("Nothing found", viewModel.EmptyStateTitle);

        viewModel.SearchText = string.Empty;
        await WaitForConditionAsync(() => service.ListCalls >= 4 && viewModel.Books.Count == 2);

        Assert.Equal(2, viewModel.Books.Count);
        Assert.Equal("2 books", viewModel.BookCountText);
        Assert.Equal("Loaded 2 book(s).", viewModel.StatusText);
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
        Assert.Null(viewModel.SelectedBook);

        await viewModel.NextPageAsync();
        Assert.Equal(2, viewModel.CurrentPage);
        Assert.Equal("Page 2", viewModel.PageLabelText);
        Assert.Null(viewModel.SelectedBook);

        await viewModel.PreviousPageAsync();
        Assert.Equal(1, viewModel.CurrentPage);
        Assert.Null(viewModel.SelectedBook);
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
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        await viewModel.LoadDetailsCommand.ExecuteAsyncForTests();

        Assert.NotNull(viewModel.SelectedBookDetails);
        Assert.Contains(viewModel.SelectedBookMetadataPairs, pair => pair.Label == "Publisher" && pair.Value == "Macmillan");
        Assert.Contains(viewModel.SelectedBookMetadataPairs, pair => pair.Label == "Size" && pair.Value == "0.00 MB");
        Assert.Equal("Aliens land only in one city.", viewModel.SelectedBookAnnotationText);
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
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        await viewModel.ExportSelectedBookAsync();

        Assert.Equal(viewModel.SelectedBook!.BookId, service.LastExportBookId);
        Assert.Equal(@"C:\Exports\alpha.epub", service.LastExportPath);
        Assert.Equal("Roadside Picnic - Arkady Strugatsky.epub", selectionService.LastSuggestedExportFileName);
        Assert.Contains("Exported", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesSanitizedTitleAndAuthorForSuggestedExportFileName()
    {
        var service = new InvalidMetadataLibraryCatalogService();
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\sanitized.fb2"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        await viewModel.ExportSelectedBookAsync();

        Assert.Equal("Roadside Picnic Director's Cut - Arkady Boris Strugatsky.fb2", selectionService.LastSuggestedExportFileName);
        Assert.Equal(@"C:\Exports\sanitized.fb2", service.LastExportPath);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_MovesSelectedBookToTrashThroughCatalogService()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
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
    public async Task LibraryBrowserViewModel_TogglesSelectionAndLoadsDetails()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        var book = viewModel.Books[0];

        await viewModel.ToggleSelectedBookAsync(book);

        Assert.Equal(book.BookId, viewModel.SelectedBook?.BookId);
        Assert.True(viewModel.ShowDetailsPanel);
        Assert.True(book.IsSelected);
        Assert.Equal("Roadside Picnic", viewModel.SelectedBookTitle);
        Assert.Equal("Arkady Strugatsky", viewModel.SelectedBookAuthorText);
        Assert.NotNull(viewModel.SelectedBookDetails);

        await viewModel.ToggleSelectedBookAsync(book);

        Assert.Null(viewModel.SelectedBook);
        Assert.False(viewModel.ShowDetailsPanel);
        Assert.False(book.IsSelected);
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
        public IReadOnlyList<string> SelectedSourcePaths { get; init; } = [];
        public string? SelectedSourceDirectory { get; init; }
        public string? SelectedWorkingDirectory { get; init; }
        public string? SelectedExportPath { get; init; }
        public string? LastSuggestedExportFileName { get; private set; }

        public Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourcePaths);

        public Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourceDirectory);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

        public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
        {
            LastSuggestedExportFileName = suggestedFileName;
            return Task.FromResult(SelectedExportPath);
        }
    }

    private static string CreateSupportedSourceFile(string sandboxRoot, string fileName)
    {
        var sourcePath = Path.Combine(sandboxRoot, fileName);
        File.WriteAllText(sourcePath, "content");
        return sourcePath;
    }

    private static async Task WaitForConditionAsync(Func<bool> condition, int timeoutMilliseconds = 3000)
    {
        var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMilliseconds);
        while (DateTime.UtcNow < deadline)
        {
            if (condition())
            {
                return;
            }

            await Task.Delay(25);
        }

        Assert.True(condition(), "Timed out waiting for the expected state.");
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

    private sealed class QueryFilteringLibraryCatalogService : ILibraryCatalogService
    {
        private static readonly IReadOnlyList<BookListItemModel> AllBooks =
        [
            new BookListItemModel
            {
                BookId = 1,
                Title = "Roadside Picnic",
                Authors = ["Arkady Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/roadside.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new BookListItemModel
            {
                BookId = 2,
                Title = "Monday Begins on Saturday",
                Authors = ["Arkady Strugatsky", "Boris Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000002/monday.fb2",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public int ListCalls { get; private set; }

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;

            if (string.IsNullOrWhiteSpace(request.Text))
            {
                return Task.FromResult(AllBooks);
            }

            var filtered = AllBooks
                .Where(book => book.Title.Contains(request.Text, StringComparison.OrdinalIgnoreCase))
                .ToArray();
            return Task.FromResult<IReadOnlyList<BookListItemModel>>(filtered);
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class CoverAwareLibraryCatalogService : ILibraryCatalogService
    {
        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<IReadOnlyList<BookListItemModel>>([
                new BookListItemModel
                {
                    BookId = 1,
                    Title = "Covered Book",
                    Authors = ["Author"],
                    Language = "en",
                    Format = BookFormatModel.Epub,
                    ManagedPath = "Books/0000000001/book.epub",
                    CoverPath = "Covers/0000000001.png",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Covered Book",
                Authors = ["Author"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/book.epub",
                CoverPath = "Covers/0000000001.png",
                SizeBytes = 1024,
                Sha256Hex = "hash",
                AddedAtUtc = DateTimeOffset.UtcNow
            });

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class InvalidMetadataLibraryCatalogService : ILibraryCatalogService
    {
        public string? LastExportPath { get; private set; }

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<IReadOnlyList<BookListItemModel>>([
                new BookListItemModel
                {
                    BookId = 41,
                    Title = "Roadside: Picnic / Director's Cut?",
                    Authors = ["Arkady*Boris|Strugatsky"],
                    Language = "en",
                    Format = BookFormatModel.Fb2,
                    ManagedPath = "Books/0000000041/book.fb2",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Roadside: Picnic / Director's Cut?",
                Authors = ["Arkady*Boris|Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000041/book.fb2",
                SizeBytes = 512,
                Sha256Hex = "invalid-metadata",
                AddedAtUtc = DateTimeOffset.UtcNow
            });

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastExportPath = destinationPath;
            return Task.FromResult<string?>(destinationPath);
        }

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class RecordingCoverImageLoader : ICoverImageLoader
    {
        public List<string> LoadedPaths { get; } = [];

        public Avalonia.Media.IImage? Load(string absolutePath)
        {
            LoadedPaths.Add(absolutePath);
            return null;
        }
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
