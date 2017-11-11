/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

#include "mtproto/sender.h"
#include "data/data_sparse_ids.h"
#include "storage/storage_sparse_ids_list.h"
#include "storage/storage_shared_media.h"
#include "base/value_ordering.h"

namespace Api {

struct SearchResult {
	std::vector<MsgId> messageIds;
	MsgRange noSkipRange;
	int fullCount = 0;
};

MTPmessages_Search PrepareSearchRequest(
	not_null<PeerData*> peer,
	Storage::SharedMediaType type,
	const QString &query,
	MsgId messageId,
	SparseIdsLoadDirection direction);

SearchResult ParseSearchResult(
	not_null<PeerData*> peer,
	Storage::SharedMediaType type,
	MsgId messageId,
	SparseIdsLoadDirection direction,
	const MTPmessages_Messages &data);

class SearchController : private MTP::Sender {
public:
	using IdsList = Storage::SparseIdsList;
	struct Query {
		using MediaType = Storage::SharedMediaType;

		PeerId peerId = 0;
		PeerId migratedPeerId = 0;
		MediaType type = MediaType::kCount;
		QString query;
		// from_id, min_date, max_date

		friend inline auto value_ordering_helper(const Query &value) {
			return std::tie(
				value.peerId,
				value.migratedPeerId,
				value.type,
				value.query);
		}

	};
	struct SavedState {
		Query query;
		IdsList peerList;
		base::optional<IdsList> migratedList;
	};

	void setQuery(const Query &query);
	bool hasInCache(const Query &query) const;

	Query query() const {
		Expects(_current != _cache.cend());
		return _current->first;
	}

	rpl::producer<SparseIdsMergedSlice> idsSlice(
		SparseIdsMergedSlice::UniversalMsgId aroundId,
		int limitBefore,
		int limitAfter);

	SavedState saveState();
	void restoreState(SavedState &&state);

private:
	struct Data {
		explicit Data(not_null<PeerData*> peer) : peer(peer) {
		}

		not_null<PeerData*> peer;
		IdsList list;
		base::flat_map<
			SparseIdsSliceBuilder::AroundData,
			rpl::lifetime> requests;
	};
	using SliceUpdate = Storage::SparseIdsSliceUpdate;

	struct CacheEntry {
		CacheEntry(const Query &query);

		Data peerData;
		base::optional<Data> migratedData;
	};

	struct CacheLess {
		inline bool operator()(const Query &a, const Query &b) const {
			return (a < b);
		}
	};
	using Cache = base::flat_map<
		Query,
		std::unique_ptr<CacheEntry>,
		CacheLess>;

	rpl::producer<SparseIdsSlice> simpleIdsSlice(
		PeerId peerId,
		MsgId aroundId,
		const Query &query,
		int limitBefore,
		int limitAfter);
	void requestMore(
		const SparseIdsSliceBuilder::AroundData &key,
		const Query &query,
		Data *listData);

	Cache _cache;
	Cache::iterator _current = _cache.end();

};

class DelayedSearchController {
public:
	DelayedSearchController();

	using Query = SearchController::Query;
	using SavedState = SearchController::SavedState;

	void setQuery(const Query &query);
	void setQuery(const Query &query, TimeMs delay);
	void setQueryFast(const Query &query);

	Query currentQuery() const {
		return _controller.query();
	}

	rpl::producer<SparseIdsMergedSlice> idsSlice(
			SparseIdsMergedSlice::UniversalMsgId aroundId,
			int limitBefore,
			int limitAfter) {
		return _controller.idsSlice(
			aroundId,
			limitBefore,
			limitAfter);
	}

	rpl::producer<> sourceChanged() const {
		return _sourceChanges.events();
	}

	SavedState saveState() {
		return _controller.saveState();
	}

	void restoreState(SavedState &&state) {
		_controller.restoreState(std::move(state));
	}

private:
	SearchController _controller;
	Query _nextQuery;
	base::Timer _timer;
	rpl::event_stream<> _sourceChanges;

};

} // namespace Api