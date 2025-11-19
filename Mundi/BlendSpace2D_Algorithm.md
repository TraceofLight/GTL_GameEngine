# BlendSpace2D 삼각분할 알고리즘 설명

## 개요

BlendSpace2D는 2D 파라미터 공간에서 여러 애니메이션을 부드럽게 블렌딩하기 위해 **Delaunay 삼각분할**과 **Barycentric Coordinates (무게중심 좌표)** 알고리즘을 사용합니다.

---

## 1. Delaunay 삼각분할 (Delaunay Triangulation)

### 1.1 목적
- 2D 공간에 배치된 애니메이션 샘플 포인트들을 삼각형으로 연결
- 최적의 삼각형 구조를 생성하여 부드러운 보간 보장

### 1.2 알고리즘: Bowyer-Watson

**구현 위치:** `BlendSpace2D.cpp` - `GenerateTriangulation()` (771-938번 라인)

**단계:**

#### Step 1: Super Triangle 생성
```cpp
// 모든 샘플을 포함하는 매우 큰 삼각형 생성
FVector2D(-10.0f, -10.0f)  // 왼쪽 아래
FVector2D(10.0f, -10.0f)   // 오른쪽 아래
FVector2D(0.0f, 20.0f)     // 위쪽
```

#### Step 2: 점진적 삼각분할
각 샘플 포인트를 하나씩 추가하면서:

1. **불량 삼각형 찾기**
   - 새 점이 외접원 내부에 있는 삼각형들 = 불량 삼각형
   - `IsPointInCircumcircle()` 함수로 판정

2. **경계(Polygon) 추출**
   - 불량 삼각형들의 변(edge) 중 공유되지 않은 변들

3. **재삼각분할**
   - 불량 삼각형 제거
   - 새 점과 Polygon의 각 변으로 새 삼각형 생성

#### Step 3: Super Triangle 제거
- Super Triangle의 정점(0, 1, 2)을 포함하는 삼각형 제거
- 인덱스 조정 (Points[3~N] → Samples[0~N-3])

### 1.3 외접원 판정 (Circumcircle Test)

**구현:** `IsPointInCircumcircle()` (842-862번 라인)

```cpp
// Determinant 방법 사용
float det =
    (ax * ax + ay * ay) * (bx * cy - cx * by) -
    (bx * bx + by * by) * (ax * cy - cx * ay) +
    (cx * cx + cy * cy) * (ax * by - bx * ay);

return det > 0.0f;  // 양수면 외접원 내부
```

### 1.4 결과 예시

**입력:** 9개 샘플
```
Sample[0]=(30.0, -45.0)   - Walk Forward Left
Sample[1]=(-30.0, 180.0)  - Walk Backward
Sample[2]=(-30.0, -90.0)  - Walk Backward Right
...
```

**출력:** 8개 삼각형
```
Triangle[0]: [1,3,4]
Triangle[1]: [2,0,5]
Triangle[2]: [3,2,5]
Triangle[3]: [4,3,6]
Triangle[4]: [3,5,6]
Triangle[5]: [5,4,6]
Triangle[6]: [0,2,7]
Triangle[7]: [7,2,8]
```

---

## 2. 무게중심 좌표 (Barycentric Coordinates)

### 2.1 개념

삼각형 ABC 내부의 점 P를 다음과 같이 표현:
```
P = w₁·A + w₂·B + w₃·C
여기서 w₁ + w₂ + w₃ = 1
```

- **w₁, w₂, w₃**: 각 정점의 가중치 (블렌드 가중치)
- **점이 정점에 가까울수록 해당 가중치가 큼**

### 2.2 계산 공식

**구현:** `CalculateBarycentricWeights()` (568-734번 라인)

```cpp
// 벡터 계산
v0 = B - A
v1 = C - A
v2 = P - A

// 내적
d00 = v0 · v0
d01 = v0 · v1
d11 = v1 · v1
d20 = v2 · v0
d21 = v2 · v1

// 분모
denom = d00 * d11 - d01 * d01

// 무게중심 좌표
w₂ = (d11 * d20 - d01 * d21) / denom
w₃ = (d00 * d21 - d01 * d20) / denom
w₁ = 1.0 - w₂ - w₃
```

### 2.3 클램핑 및 정규화

#### 문제: 삼각형 외부의 점
- 무게중심 좌표가 음수가 나올 수 있음
- 예: w₁=0.980, w₂=-0.565, w₃=0.585

#### 해결: 클램핑 후 정규화
```cpp
// 1. 음수를 0으로 클램핑
w₁ = max(0.0, w₁)
w₂ = max(0.0, w₂)
w₃ = max(0.0, w₃)

// 2. 정규화 (합 = 1.0)
sum = w₁ + w₂ + w₃
w₁ = w₁ / sum
w₂ = w₂ / sum
w₃ = w₃ / sum
```

**결과:** w₁=0.626, w₂=0.000, w₃=0.374

---

## 3. 삼각형 선택 알고리즘

### 3.1 포함 삼각형 찾기

**구현:** `FindContainingTriangle()` (512-629번 라인)

**핵심 아이디어:**
- **클램핑 전 원본 가중치**로 삼각형 내부/외부 판정
- 클램핑된 값을 사용하면 삼각형 밖의 점도 내부로 인식됨

```cpp
for each Triangle:
    // 원본 무게중심 좌표 계산 (클램핑 안 함)
    계산: w₁_raw, w₂_raw, w₃_raw

    // 내부 판정
    if (w₁_raw >= -0.01 && w₂_raw >= -0.01 && w₃_raw >= -0.01):
        // 이 삼각형 내부에 점이 있음!
        가중치 클램핑 및 정규화
        return true
```

### 3.2 예시

**Point = (0.572, 0.212)** 찾기

```
Triangle[0] (0,1,2): Raw Weights=(0.980, -0.565, 0.585)
→ w₂ = -0.565 < -0.01 ❌ 스킵

Triangle[1] (2,0,5): Raw Weights=(0.850, 0.100, 0.050)
→ 모두 >= -0.01 ✅ 발견!
→ 클램핑 후: (0.850, 0.100, 0.050)
```

### 3.3 Fallback: 경계 밖의 점

**구현:** `FindContainingTriangle()` (598-644번 라인)

모든 삼각형을 검사했는데도 못 찾으면:

1. **가장 가까운 삼각형 찾기**
   ```cpp
   for each Triangle:
       center = (A + B + C) / 3
       dist = distance(Point, center)

   select Triangle with minimum dist
   ```

2. **무게중심 좌표 계산**
   - 음수 가중치가 나와도 클램핑 및 정규화

---

## 4. 전체 블렌딩 흐름

### 4.1 GetBlendWeights() 함수

**구현:** `BlendSpace2D.cpp` (291-406번 라인)

```cpp
Input: BlendParameter (예: Speed=28.8, Direction=-103.7)

Step 1: 파라미터 정규화
    NormParam = Normalize(BlendParameter)
    // (28.8, -103.7) → (0.572, 0.212)

Step 2: 특수 케이스 처리
    if (샘플 0개): return
    if (샘플 1개): return [Index=0, Weight=1.0]
    if (샘플 2개): return 선형 보간

Step 3: 삼각형 찾기
    FindClosestTriangle(NormParam)
    → Triangle[0,1,2], Weights=[0.626, 0.000, 0.374]

Step 4: 유효한 샘플만 추가
    for each weight:
        if weight > 0.001:
            add to output

Step 5: 정규화
    sum = Σ weights
    weights /= sum

Output:
    Indices = [0, 2]
    Weights = [0.626, 0.374]
```

### 4.2 포즈 블렌딩

**구현:** `AnimNode_BlendSpace2D.cpp` - `Evaluate()` (213-293번 라인)

```cpp
Step 1: 가중치 계산
    GetBlendWeights() → Indices, Weights

Step 2: 각 샘플의 포즈 샘플링
    for each Index:
        pose[i] = GetPoseFromAnimSequence(
            Sample[Index].Animation,
            SampleAnimTimes[Index]
        )

Step 3: 포즈 블렌딩
    FinalPose = Σ (pose[i] * weight[i])
```

---

## 5. 핵심 개선 사항

### 5.1 문제: 잘못된 삼각형 선택

**이전 구현:**
```cpp
// ❌ 클램핑된 값으로 판정
CalculateBarycentricWeights()  // 음수 → 0
if (w₁ >= 0 && w₂ >= 0 && w₃ >= 0)  // 항상 참!
```

**결과:**
- 삼각형 밖의 점도 내부로 인식
- 항상 첫 번째 삼각형(Triangle[0])만 선택
- 꼭지점에 가까워도 가중치 0.6, 0.4

### 5.2 해결: 원본 가중치로 판정

**현재 구현:**
```cpp
// ✅ 원본 값으로 판정
직접 계산: w₁_raw, w₂_raw, w₃_raw  // 클램핑 안 함
if (w₁_raw >= -0.01 && w₂_raw >= -0.01 && w₃_raw >= -0.01)
    CalculateBarycentricWeights()  // 이제 클램핑
```

**결과:**
- 올바른 삼각형 선택
- Point 위치에 따라 적절한 삼각형 사용
- 꼭지점에 가까우면 가중치 거의 1.0

---

## 6. 로드 시 삼각분할 재생성

### 6.1 문제
JSON 파일에서 로드 시 삼각분할 데이터가 없거나 손상될 수 있음

### 6.2 해결

**구현:** `LoadFromFile()` (762-786번 라인)

```cpp
UBlendSpace2D* LoadFromFile(FilePath):
    Deserialize(JsonData)

    // 로드 후 삼각분할 재생성
    if (GetNumSamples() >= 3):
        GenerateTriangulation()

    return BlendSpace
```

---

## 7. 알고리즘 비교

| 특징 | Barycentric | Inverse Distance | Grid Bilinear |
|------|-------------|------------------|---------------|
| **샘플 개수** | 3개 (삼각형) | 3개 | 4개 (사각형) |
| **삼각분할 필요** | ✅ 필요 | ✅ 필요 | ❌ 불필요 |
| **블렌딩 특성** | 부드러움 | 샤프함 | 매우 부드러움 |
| **꼭지점 가중치** | 중간 | 거의 100% | 중간 |
| **언리얼 사용** | ✅ (Triangulation) | ❌ | ✅ (Grid, 기본) |

---

## 8. 주요 함수 요약

| 함수 | 위치 | 역할 |
|------|------|------|
| `GenerateTriangulation()` | BlendSpace2D.cpp:771 | Delaunay 삼각분할 생성 |
| `IsPointInCircumcircle()` | BlendSpace2D.cpp:842 | 외접원 판정 |
| `GetBlendWeights()` | BlendSpace2D.cpp:291 | 블렌드 가중치 계산 |
| `FindClosestTriangle()` | BlendSpace2D.cpp:427 | 적절한 삼각형 찾기 |
| `FindContainingTriangle()` | BlendSpace2D.cpp:512 | 점을 포함하는 삼각형 찾기 |
| `CalculateBarycentricWeights()` | BlendSpace2D.cpp:568 | 무게중심 좌표 계산 |

---

## 9. 디버그 로그 예시

### 삼각분할 생성
```
[Triangulation] Starting with 9 samples...
[Triangulation] Complete: 9 samples -> 8 triangles
  Triangle[0]: Indices[1,3,4]
    Positions: [-30.0,180.0], [-30.0,90.0], [30.0,90.0]
  Triangle[1]: Indices[2,0,5]
    Positions: [-30.0,-90.0], [30.0,-45.0], [30.0,0.0]
  ...
```

### 삼각형 검색
```
[FindContainingTriangle] Searching for Point=(0.572, 0.212) in 8 triangles
  Triangle[0] (1,3,4): Raw Weights=(0.980, -0.565, 0.585)  ❌ 스킵
  Triangle[1] (2,0,5): Raw Weights=(0.850, 0.100, 0.050)  ✅ 발견
  -> Found! Triangle[1] contains the point
```

### 최종 블렌딩
```
[BlendSpace2D] Param=(28.8, -103.7) -> 2 samples blending:
  Sample[0] at (30.0, -45.0): Weight=0.850
  Sample[5] at (30.0, 0.0): Weight=0.100
  Sample[2] at (-30.0, -90.0): Weight=0.050
```

---

## 10. 참고 자료

- **Delaunay Triangulation:** Bowyer-Watson 알고리즘
- **Barycentric Coordinates:** 무게중심 좌표계
- **Unreal Engine:** BlendSpace with Triangulation mode
- **파일:**
  - `BlendSpace2D.h` / `BlendSpace2D.cpp`
  - `AnimNode_BlendSpace2D.h` / `AnimNode_BlendSpace2D.cpp`
  - `BlendSpace2DEditorWindow.cpp`
