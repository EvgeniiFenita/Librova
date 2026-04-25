#include "Models/BookDetailsModel.hpp"

#include "Adapters/QtStringConversions.hpp"
#include "Domain/BookFormat.hpp"

namespace LibrovaQt {

namespace {

QStringList ToStringList(const std::vector<std::string>& values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const auto& value : values)
    {
        result.append(qtFromUtf8(value));
    }
    return result;
}

} // namespace

BookDetailsModel::BookDetailsModel(QObject* parent)
    : QObject(parent)
{
}

void BookDetailsModel::clear()
{
    m_hasBook = false;
    m_bookId = 0;
    m_title.clear();
    m_authors.clear();
    m_language.clear();
    m_seriesName.clear();
    m_seriesIndex = 0.0;
    m_publisher.clear();
    m_year = 0;
    m_isbn.clear();
    m_tags.clear();
    m_genres.clear();
    m_description.clear();
    m_identifier.clear();
    m_format.clear();
    m_managedPath.clear();
    m_coverPath.clear();
    m_sizeBytes = 0;
    m_sha256Hex.clear();
    m_collectionIds.clear();
    m_collectionNames.clear();
    Q_EMIT changed();
}

void BookDetailsModel::setDetails(const Librova::Application::SBookDetails& details)
{
    m_hasBook = true;
    m_bookId = static_cast<qint64>(details.Id.Value);
    m_title = qtFromUtf8(details.TitleUtf8);
    m_authors = ToStringList(details.AuthorsUtf8).join(QStringLiteral(", "));
    m_language = qtFromUtf8(details.Language);
    m_seriesName = details.SeriesUtf8.has_value() ? qtFromUtf8(*details.SeriesUtf8) : QString{};
    m_seriesIndex = details.SeriesIndex.value_or(0.0);
    m_publisher = details.PublisherUtf8.has_value() ? qtFromUtf8(*details.PublisherUtf8) : QString{};
    m_year = details.Year.value_or(0);
    m_isbn = details.Isbn.has_value() ? qtFromUtf8(*details.Isbn) : QString{};
    m_tags = ToStringList(details.TagsUtf8);
    m_genres = ToStringList(details.GenresUtf8);
    m_description =
        details.DescriptionUtf8.has_value() ? qtFromUtf8(*details.DescriptionUtf8) : QString{};
    m_identifier = details.Identifier.has_value() ? qtFromUtf8(*details.Identifier) : QString{};
    m_format = qtFromUtf8(std::string(Librova::Domain::ToString(details.Format)));
    m_managedPath = qtFromPath(details.ManagedPath);
    m_coverPath = details.CoverPath.has_value() ? qtFromPath(*details.CoverPath) : QString{};
    m_sizeBytes = static_cast<qint64>(details.SizeBytes);
    m_sha256Hex = qtFromUtf8(details.Sha256Hex);

    m_collectionIds.clear();
    m_collectionNames.clear();
    for (const auto& collection : details.Collections)
    {
        m_collectionIds.append(QString::number(collection.Id));
        m_collectionNames.append(qtFromUtf8(collection.NameUtf8));
    }

    Q_EMIT changed();
}

} // namespace LibrovaQt
